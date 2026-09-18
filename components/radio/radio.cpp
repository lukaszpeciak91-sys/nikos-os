#include "radio/radio.hpp"

#include <atomic>
#include <cstring>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

namespace {

constexpr char kTag[] = "radio";
constexpr std::size_t kEventQueueDepth = 12;
constexpr nikos::radio::MacAddress kBroadcastMac = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

QueueHandle_t s_event_queue = nullptr;
std::atomic<std::uint32_t> s_dropped_event_count{0};

bool check_ok(esp_err_t result, const char* operation)
{
    if (result == ESP_OK) {
        return true;
    }

    ESP_LOGE(kTag, "%s failed: %s", operation, esp_err_to_name(result));
    return false;
}

void queue_event(const nikos::radio::Event& event)
{
    if (xQueueSend(s_event_queue, &event, 0) != pdTRUE) {
        s_dropped_event_count.fetch_add(1, std::memory_order_relaxed);
    }
}

void send_callback(
    const esp_now_send_info_t* tx_info,
    esp_now_send_status_t status)
{
    if (s_event_queue == nullptr || tx_info == nullptr || tx_info->des_addr == nullptr) {
        return;
    }

    nikos::radio::Event event;
    event.type = nikos::radio::EventType::TxResult;
    std::memcpy(
        event.tx.destination.data(),
        tx_info->des_addr,
        nikos::radio::kMacSize);
    event.tx.success = status == ESP_NOW_SEND_SUCCESS;

    queue_event(event);
}

void receive_callback(
    const esp_now_recv_info_t* info,
    const std::uint8_t* data,
    int data_length)
{
    if (s_event_queue == nullptr
        || info == nullptr
        || info->src_addr == nullptr
        || info->des_addr == nullptr
        || data == nullptr
        || data_length <= 0
        || static_cast<std::size_t>(data_length) > nikos::radio::kMaxFrameSize) {
        return;
    }

    nikos::radio::Event event;
    event.type = nikos::radio::EventType::Rx;
    event.rx.received_time_us =
        static_cast<std::uint64_t>(esp_timer_get_time());
    std::memcpy(
        event.rx.source.data(),
        info->src_addr,
        nikos::radio::kMacSize);
    std::memcpy(
        event.rx.destination.data(),
        info->des_addr,
        nikos::radio::kMacSize);
    std::memcpy(
        event.rx.data.data(),
        data,
        static_cast<std::size_t>(data_length));
    event.rx.length = static_cast<std::size_t>(data_length);

    if (info->rx_ctrl != nullptr) {
        event.rx.rssi = info->rx_ctrl->rssi;
        event.rx.has_rssi = true;
    }

    queue_event(event);
}

}  // namespace

namespace nikos::radio {

bool RadioService::begin(std::uint8_t channel, Mode mode)
{
    if (initialized_) {
        return true;
    }

    s_event_queue = xQueueCreate(kEventQueueDepth, sizeof(Event));
    if (s_event_queue == nullptr) {
        ESP_LOGE(kTag, "Failed to create ESP-NOW event queue");
        return false;
    }
    s_dropped_event_count.store(0, std::memory_order_relaxed);

    if (!check_ok(esp_netif_init(), "esp_netif_init")) {
        return false;
    }
    if (!check_ok(esp_event_loop_create_default(), "esp_event_loop_create_default")) {
        return false;
    }

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    if (!check_ok(esp_wifi_init(&wifi_config), "esp_wifi_init")
        || !check_ok(esp_wifi_set_storage(WIFI_STORAGE_RAM), "esp_wifi_set_storage")
        || !check_ok(esp_wifi_set_mode(WIFI_MODE_STA), "esp_wifi_set_mode")
        || !check_ok(esp_wifi_start(), "esp_wifi_start")
        || !check_ok(esp_wifi_set_ps(WIFI_PS_NONE), "esp_wifi_set_ps")
        || !check_ok(
            esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE),
            "esp_wifi_set_channel")) {
        return false;
    }

    channel_ = channel;
    if (!apply_mode(mode)) {
        return false;
    }

    if (!check_ok(esp_now_init(), "esp_now_init")
        || !check_ok(esp_now_register_send_cb(send_callback), "esp_now_register_send_cb")
        || !check_ok(esp_now_register_recv_cb(receive_callback), "esp_now_register_recv_cb")
        || !add_broadcast_peer()
        || !check_ok(
            esp_wifi_get_mac(WIFI_IF_STA, self_mac_.data()),
            "esp_wifi_get_mac")) {
        return false;
    }

    initialized_ = true;
    ESP_LOGI(
        kTag,
        "ESP-NOW ready on channel %u in %s mode",
        static_cast<unsigned>(channel_),
        mode_name(mode_));
    return true;
}

bool RadioService::apply_mode(Mode mode)
{
    const std::uint8_t protocols =
        mode == Mode::Lr
            ? WIFI_PROTOCOL_LR
            : static_cast<std::uint8_t>(
                WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);

    if (!check_ok(
            esp_wifi_set_protocol(WIFI_IF_STA, protocols),
            "esp_wifi_set_protocol")) {
        return false;
    }

    mode_ = mode;
    return true;
}

bool RadioService::set_mode(Mode mode)
{
    if (!initialized_ || mode == mode_) {
        return initialized_;
    }

    if (!apply_mode(mode)) {
        return false;
    }

    ESP_LOGI(kTag, "Radio mode changed to %s", mode_name(mode_));
    return true;
}

Mode RadioService::mode() const
{
    return mode_;
}

std::uint8_t RadioService::channel() const
{
    return channel_;
}

const MacAddress& RadioService::self_mac() const
{
    return self_mac_;
}

bool RadioService::add_broadcast_peer()
{
    if (esp_now_is_peer_exist(kBroadcastMac.data())) {
        return true;
    }

    esp_now_peer_info_t peer{};
    std::memcpy(peer.peer_addr, kBroadcastMac.data(), kMacSize);
    peer.channel = channel_;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    return check_ok(esp_now_add_peer(&peer), "esp_now_add_peer(broadcast)");
}

bool RadioService::set_peer(const MacAddress& mac)
{
    if (!initialized_ || mac_equal(mac, self_mac_) || mac_equal(mac, kBroadcastMac)) {
        return false;
    }

    if (has_peer_ && mac_equal(mac, peer_mac_) && esp_now_is_peer_exist(mac.data())) {
        return true;
    }

    clear_peer();

    esp_now_peer_info_t peer{};
    std::memcpy(peer.peer_addr, mac.data(), kMacSize);
    peer.channel = channel_;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;

    const esp_err_t result = esp_now_add_peer(&peer);
    if (!check_ok(result, "esp_now_add_peer(unicast)")) {
        return false;
    }

    peer_mac_ = mac;
    has_peer_ = true;
    ESP_LOGI(
        kTag,
        "Registered peer %02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
    return true;
}

void RadioService::clear_peer()
{
    if (!has_peer_) {
        return;
    }

    const esp_err_t result = esp_now_del_peer(peer_mac_.data());
    if (result != ESP_OK && result != ESP_ERR_ESPNOW_NOT_FOUND) {
        ESP_LOGW(kTag, "esp_now_del_peer failed: %s", esp_err_to_name(result));
    }

    peer_mac_.fill(0);
    has_peer_ = false;
}

bool RadioService::has_peer() const
{
    return has_peer_;
}

const MacAddress& RadioService::peer_mac() const
{
    return peer_mac_;
}

bool RadioService::send_broadcast(const std::uint8_t* data, std::size_t length)
{
    if (!initialized_ || data == nullptr || length == 0 || length > kMaxFrameSize) {
        return false;
    }

    const esp_err_t result = esp_now_send(kBroadcastMac.data(), data, length);
    if (result != ESP_OK) {
        ESP_LOGW(kTag, "Broadcast send request failed: %s", esp_err_to_name(result));
        return false;
    }
    return true;
}

bool RadioService::send_peer(const std::uint8_t* data, std::size_t length)
{
    if (!initialized_
        || !has_peer_
        || data == nullptr
        || length == 0
        || length > kMaxFrameSize) {
        return false;
    }

    const esp_err_t result = esp_now_send(peer_mac_.data(), data, length);
    if (result != ESP_OK) {
        ESP_LOGW(kTag, "Unicast send request failed: %s", esp_err_to_name(result));
        return false;
    }
    return true;
}

bool RadioService::poll(Event& event)
{
    return s_event_queue != nullptr
        && xQueueReceive(s_event_queue, &event, 0) == pdTRUE;
}

std::uint32_t RadioService::dropped_event_count() const
{
    return s_dropped_event_count.load(std::memory_order_relaxed);
}

bool mac_equal(const MacAddress& left, const MacAddress& right)
{
    return left == right;
}

const char* mode_name(Mode mode)
{
    return mode == Mode::Lr ? "LR" : "NORMAL";
}

}  // namespace nikos::radio
