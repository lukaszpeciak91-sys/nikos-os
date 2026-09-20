#include "messaging/messaging_service.hpp"

#include <array>

#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"

namespace {

constexpr char kTag[] = "messaging";

bool is_notifiable(nikos::communicator_protocol::MessageType type)
{
    using nikos::communicator_protocol::MessageType;
    return type == MessageType::PresetMessage
        || type == MessageType::PresetResponse
        || type == MessageType::Ring;
}

nikos::messaging::IncomingKind to_incoming_kind(
    nikos::communicator_protocol::MessageType type)
{
    using nikos::communicator_protocol::MessageType;
    switch (type) {
        case MessageType::PresetResponse:
            return nikos::messaging::IncomingKind::PresetResponse;
        case MessageType::Ring:
            return nikos::messaging::IncomingKind::Ring;
        case MessageType::PresetMessage:
        default:
            return nikos::messaging::IncomingKind::PresetMessage;
    }
}

nikos::messaging::DeliveryKind to_delivery_kind(
    nikos::communicator_protocol::MessageType type)
{
    using nikos::communicator_protocol::MessageType;
    switch (type) {
        case MessageType::PresetResponse:
            return nikos::messaging::DeliveryKind::PresetResponse;
        case MessageType::Ring:
            return nikos::messaging::DeliveryKind::Ring;
        case MessageType::PresetMessage:
        default:
            return nikos::messaging::DeliveryKind::PresetMessage;
    }
}

}  // namespace

namespace nikos::messaging {

Service::Service(radio::RadioService& radio)
    : radio_(radio)
{
}

bool Service::begin(const Config& config)
{
    if (started_) {
        return true;
    }

    if (config.presence_interval_ms == 0
        || config.retry_interval_ms == 0
        || config.foreground_rx.interval_ms == 0
        || config.foreground_rx.wake_window_ms == 0
        || config.foreground_rx.wake_window_ms > config.foreground_rx.interval_ms
        || config.foreground_rx.reachability_timeout_ms == 0
        || config.background_rx.interval_ms == 0
        || config.background_rx.wake_window_ms == 0
        || config.background_rx.wake_window_ms > config.background_rx.interval_ms
        || config.background_rx.reachability_timeout_ms == 0) {
        ESP_LOGE(kTag, "Invalid messaging configuration");
        return false;
    }

    config_ = config;
    rx_profile_ = RxProfile::Background;

    started_ = true;
    if (!start_transport()) {
        started_ = false;
        return false;
    }

    // Seed logical IDs only after Wi-Fi/radio startup, when RF entropy is available.
    next_message_id_ = esp_random();
    if (next_message_id_ == 0) {
        next_message_id_ = 1;
    }

    return true;
}

void Service::update()
{
    if (!started_ || !transport_active_) {
        return;
    }

    process_radio_events();

    const std::uint32_t now = now_ms();
    if (last_presence_tx_ms_ == 0
        || now - last_presence_tx_ms_ >= current_presence_delay_ms_) {
        send_presence(now);
    }

    if (outgoing_.active
        && peer_reachable()
        && (outgoing_.last_send_ms == 0
            || now - outgoing_.last_send_ms >= config_.retry_interval_ms)) {
        send_outgoing(now);
    }
}

bool Service::pause_transport()
{
    if (!started_ || !transport_active_) {
        return true;
    }

    const bool stopped = radio_.stop();
    transport_active_ = false;
    return stopped;
}

bool Service::resume_transport()
{
    if (!started_) {
        return false;
    }
    if (transport_active_) {
        return true;
    }

    return start_transport();
}

bool Service::transport_active() const
{
    return transport_active_;
}

bool Service::set_rx_profile(RxProfile profile)
{
    if (!started_) {
        rx_profile_ = profile;
        return true;
    }

    if (!transport_active_) {
        rx_profile_ = profile;
        return true;
    }

    const RxProfile previous = rx_profile_;
    rx_profile_ = profile;
    if (!radio_.set_rx_power(rx_power_for(profile))) {
        rx_profile_ = previous;
        return false;
    }

    return true;
}

RxProfile Service::rx_profile() const
{
    return rx_profile_;
}

bool Service::send_preset_message(std::uint16_t message_id)
{
    return start_outgoing(
        communicator_protocol::MessageType::PresetMessage,
        message_id,
        0);
}

bool Service::send_preset_response(
    std::uint16_t response_id,
    std::uint32_t reference_message_id)
{
    return start_outgoing(
        communicator_protocol::MessageType::PresetResponse,
        response_id,
        reference_message_id);
}

bool Service::send_ring()
{
    return start_outgoing(
        communicator_protocol::MessageType::Ring,
        0,
        0);
}

bool Service::outgoing_pending() const
{
    return outgoing_.active;
}

std::uint32_t Service::outgoing_logical_message_id() const
{
    return outgoing_.active ? outgoing_.message.message_id : 0;
}

bool Service::peer_known() const
{
    return peer_known_;
}

bool Service::peer_reachable() const
{
    const RxSchedule& schedule =
        rx_profile_ == RxProfile::Foreground
            ? config_.foreground_rx
            : config_.background_rx;

    return peer_known_
        && last_peer_rx_ms_ != 0
        && now_ms() - last_peer_rx_ms_
            <= schedule.reachability_timeout_ms;
}

const radio::MacAddress& Service::self_mac() const
{
    return radio_.self_mac();
}

const radio::MacAddress& Service::peer_mac() const
{
    return peer_mac_;
}

bool Service::latest_peer_rssi(std::int8_t& rssi) const
{
    if (!latest_peer_rssi_valid_) {
        return false;
    }

    rssi = latest_peer_rssi_;
    return true;
}

bool Service::peek_incoming(IncomingMessage& message) const
{
    if (incoming_count_ == 0) {
        return false;
    }

    message = incoming_queue_[incoming_head_];
    return true;
}

bool Service::consume_incoming(std::uint32_t logical_message_id)
{
    if (incoming_count_ == 0
        || incoming_queue_[incoming_head_].logical_message_id
            != logical_message_id) {
        return false;
    }

    incoming_head_ = (incoming_head_ + 1U) % kIncomingQueueDepth;
    --incoming_count_;
    return true;
}

bool Service::poll_delivery(DeliveryReceipt& receipt)
{
    if (!delivery_ready_) {
        return false;
    }

    receipt = delivery_receipt_;
    delivery_ready_ = false;
    return true;
}

bool Service::start_transport()
{
    if (!radio_.begin(config_.channel, config_.mode, rx_power_for(rx_profile_))) {
        return false;
    }

    if (peer_known_ && !radio_.set_peer(peer_mac_)) {
        radio_.stop();
        return false;
    }

    transport_active_ = true;
    last_presence_tx_ms_ = 0;
    current_presence_delay_ms_ = 0;
    outgoing_.last_send_ms = 0;
    return true;
}

radio::RxPowerConfig Service::rx_power_for(RxProfile profile) const
{
    const RxSchedule& schedule =
        profile == RxProfile::Foreground
            ? config_.foreground_rx
            : config_.background_rx;

    radio::RxPowerConfig power;
    power.mode = radio::RxPowerMode::DutyCycled;
    power.wake_interval_ms = schedule.interval_ms;
    power.wake_window_ms = schedule.wake_window_ms;
    return power;
}

void Service::process_radio_events()
{
    radio::Event event;
    while (radio_.poll(event)) {
        if (event.type == radio::EventType::Rx) {
            process_rx(event.rx);
        }
    }
}

void Service::process_rx(const radio::RxEvent& event)
{
    if (radio::mac_equal(event.source, radio_.self_mac())) {
        return;
    }

    communicator_protocol::Message message;
    if (!communicator_protocol::decode(
            event.data.data(),
            event.length,
            message)) {
        return;
    }

    if (!peer_known_) {
        if (!radio_.set_peer(event.source)) {
            return;
        }

        peer_known_ = true;
        peer_mac_ = event.source;
        ESP_LOGI(
            kTag,
            "Learned messaging peer %02X:%02X:%02X:%02X:%02X:%02X",
            peer_mac_[0],
            peer_mac_[1],
            peer_mac_[2],
            peer_mac_[3],
            peer_mac_[4],
            peer_mac_[5]);
    } else if (!radio::mac_equal(event.source, peer_mac_)) {
        return;
    }

    record_peer_rx(event);

    if (message.type == communicator_protocol::MessageType::Presence) {
        return;
    }

    if (message.type == communicator_protocol::MessageType::Ack) {
        if (outgoing_.active
            && message.reference_id == outgoing_.message.message_id) {
            publish_delivery();
            outgoing_ = OutgoingState{};
        }
        return;
    }

    if (!is_notifiable(message.type)) {
        return;
    }

    if (is_duplicate(message.message_id)) {
        // Duplicate copies are still ACKed but are not surfaced again.
        send_ack(message.message_id);
        return;
    }

    IncomingMessage incoming;
    incoming.kind = to_incoming_kind(message.type);
    incoming.logical_message_id = message.message_id;
    incoming.reference_message_id = message.reference_id;
    incoming.value_id = message.value_id;

    // Do not application-ACK/dedupe a new logical message until it has been
    // retained locally. If this small queue is full, the sender will retry.
    if (!enqueue_incoming(incoming)) {
        return;
    }

    remember_received(message.message_id);
    send_ack(message.message_id);
}

void Service::record_peer_rx(const radio::RxEvent& event)
{
    last_peer_rx_ms_ =
        static_cast<std::uint32_t>(event.received_time_us / 1000U);

    latest_peer_rssi_valid_ = event.has_rssi;
    if (event.has_rssi) {
        latest_peer_rssi_ = event.rssi;
    }
}

void Service::send_presence(std::uint32_t now_ms)
{
    communicator_protocol::Message message;
    message.type = communicator_protocol::MessageType::Presence;

    std::array<std::uint8_t, communicator_protocol::kWireSize> wire{};
    if (communicator_protocol::encode(message, wire.data(), wire.size())) {
        radio_.send_broadcast(wire.data(), wire.size());
    }

    const std::uint32_t jitter =
        config_.presence_jitter_ms == 0
            ? 0
            : esp_random()
                % (static_cast<std::uint32_t>(config_.presence_jitter_ms) + 1U);
    current_presence_delay_ms_ = config_.presence_interval_ms + jitter;
    last_presence_tx_ms_ = now_ms;
}

void Service::send_ack(std::uint32_t reference_message_id)
{
    if (!peer_known_) {
        return;
    }

    communicator_protocol::Message message;
    message.type = communicator_protocol::MessageType::Ack;
    message.message_id = next_logical_message_id();
    message.reference_id = reference_message_id;

    std::array<std::uint8_t, communicator_protocol::kWireSize> wire{};
    if (communicator_protocol::encode(message, wire.data(), wire.size())) {
        radio_.send_peer(wire.data(), wire.size());
    }
}

bool Service::start_outgoing(
    communicator_protocol::MessageType type,
    std::uint16_t value_id,
    std::uint32_t reference_message_id)
{
    if (!started_ || !peer_known_ || outgoing_.active) {
        return false;
    }

    outgoing_ = OutgoingState{};
    outgoing_.active = true;
    outgoing_.message.type = type;
    outgoing_.message.message_id = next_logical_message_id();
    outgoing_.message.reference_id = reference_message_id;
    outgoing_.message.value_id = value_id;

    if (transport_active_) {
        send_outgoing(now_ms());
    }

    return true;
}

void Service::send_outgoing(std::uint32_t now_ms)
{
    if (!transport_active_
        || !outgoing_.active
        || !peer_known_
        || !peer_reachable()) {
        return;
    }

    std::array<std::uint8_t, communicator_protocol::kWireSize> wire{};
    if (!communicator_protocol::encode(
            outgoing_.message,
            wire.data(),
            wire.size())) {
        return;
    }

    radio_.send_peer(wire.data(), wire.size());
    outgoing_.last_send_ms = now_ms;
    ++outgoing_.attempts;
}

bool Service::is_duplicate(std::uint32_t logical_message_id) const
{
    if (logical_message_id == 0) {
        return false;
    }

    for (std::size_t index = 0; index < recent_received_count_; ++index) {
        if (recent_received_ids_[index] == logical_message_id) {
            return true;
        }
    }

    return false;
}

void Service::remember_received(std::uint32_t logical_message_id)
{
    if (logical_message_id == 0) {
        return;
    }

    recent_received_ids_[recent_received_next_] = logical_message_id;
    recent_received_next_ = (recent_received_next_ + 1U) % kDedupeDepth;
    if (recent_received_count_ < kDedupeDepth) {
        ++recent_received_count_;
    }
}

bool Service::enqueue_incoming(const IncomingMessage& message)
{
    if (incoming_count_ == kIncomingQueueDepth) {
        return false;
    }

    const std::size_t tail =
        (incoming_head_ + incoming_count_) % kIncomingQueueDepth;
    incoming_queue_[tail] = message;
    ++incoming_count_;
    return true;
}

void Service::publish_delivery()
{
    delivery_receipt_.kind = to_delivery_kind(outgoing_.message.type);
    delivery_receipt_.logical_message_id = outgoing_.message.message_id;
    delivery_ready_ = true;
}

std::uint32_t Service::next_logical_message_id()
{
    std::uint32_t current = next_message_id_++;
    if (current == 0) {
        current = next_message_id_++;
    }
    if (next_message_id_ == 0) {
        ++next_message_id_;
    }
    return current;
}

std::uint32_t Service::now_ms() const
{
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000U);
}

}  // namespace nikos::messaging
