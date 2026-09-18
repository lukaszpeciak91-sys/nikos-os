#include "radiolab/radiolab_app.hpp"

#include <array>
#include <cinttypes>
#include <cstdio>

#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "protocol/protocol.hpp"

namespace {

constexpr char kTag[] = "radiolab";

constexpr std::uint32_t kDiscoveryIntervalMs = 2000;
constexpr std::uint32_t kLivePingIntervalMs = 1500;
constexpr std::uint32_t kPingTimeoutMs = 1200;
constexpr std::uint32_t kPeerLostMs = 7000;
constexpr std::uint32_t kReachableMs = 4000;
constexpr std::uint32_t kStaleMs = 12000;
constexpr std::uint32_t kRenderIntervalMs = 250;

nikos::protocol::RadioMode to_wire_mode(nikos::radio::Mode mode)
{
    return mode == nikos::radio::Mode::Lr
        ? nikos::protocol::RadioMode::Lr
        : nikos::protocol::RadioMode::Normal;
}

nikos::radio::Mode from_wire_mode(nikos::protocol::RadioMode mode)
{
    return mode == nikos::protocol::RadioMode::Lr
        ? nikos::radio::Mode::Lr
        : nikos::radio::Mode::Normal;
}

void format_mac(const nikos::radio::MacAddress& mac, char* output, std::size_t output_size)
{
    std::snprintf(
        output,
        output_size,
        "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0],
        mac[1],
        mac[2],
        mac[3],
        mac[4],
        mac[5]);
}

}  // namespace

namespace nikos::radiolab {

RadioLabApp::RadioLabApp(board::Board& board, radio::RadioService& radio)
    : board_(board), radio_(radio)
{
}

void RadioLabApp::begin()
{
    char mac[18]{};
    format_mac(radio_.self_mac(), mac, sizeof(mac));

    ESP_LOGI(
        kTag,
        "RadioLab v0.1 started, MAC=%s channel=%u mode=%s",
        mac,
        static_cast<unsigned>(radio_.channel()),
        radio::mode_name(radio_.mode()));

    send_discovery();
    const std::uint32_t now = now_ms();
    last_discovery_tx_ms_ = now;
    render(now);
}

void RadioLabApp::update()
{
    const std::uint32_t now = now_ms();

    process_input(board_.poll_input(), now);
    process_radio_events();
    process_timers(now);

    if (now - last_render_ms_ >= kRenderIntervalMs) {
        render(now);
    }
}

void RadioLabApp::process_input(
    const board::InputState& input,
    std::uint32_t now_ms)
{
    if (input.b_short) {
        const auto next =
            (static_cast<std::uint8_t>(action_) + 1U)
            % static_cast<std::uint8_t>(Action::Count);
        action_ = static_cast<Action>(next);
        render(now_ms);
    }

    if (input.b_long) {
        action_ = Action::Ping;
        render(now_ms);
    }

    if (!input.a_short) {
        return;
    }

    switch (action_) {
        case Action::Ping:
            send_ping();
            break;
        case Action::Hello:
            send_hello();
            break;
        case Action::Live:
            live_enabled_ = !live_enabled_;
            if (live_enabled_) {
                last_live_ping_ms_ = 0;
            }
            break;
        case Action::Mode:
            toggle_mode();
            break;
        case Action::Count:
        default:
            break;
    }

    render(now_ms);
}

void RadioLabApp::process_radio_events()
{
    radio::Event event;
    while (radio_.poll(event)) {
        if (event.type == radio::EventType::Rx) {
            process_rx(event.rx);
        } else {
            process_tx(event.tx);
        }
    }
}

void RadioLabApp::process_rx(const radio::RxEvent& event)
{
    if (radio::mac_equal(event.source, radio_.self_mac())) {
        return;
    }

    protocol::Message message;
    if (!protocol::decode(event.data.data(), event.length, message)) {
        return;
    }

    if (message.mode != to_wire_mode(radio_.mode())) {
        return;
    }

    if (message.type == protocol::MessageType::Discovery) {
        if (!peer_known_) {
            if (!radio_.set_peer(event.source)) {
                return;
            }
            peer_known_ = true;
            peer_mac_ = event.source;
            last_valid_rx_ms_ = 0;
            matching_ack_rssi_valid_ = false;
            peer_ping_rssi_valid_ = false;
            last_rtt_ms_ = -1;

            char mac[18]{};
            format_mac(peer_mac_, mac, sizeof(mac));
            ESP_LOGI(kTag, "Compatible peer discovered: %s", mac);
        }

        if (radio::mac_equal(event.source, peer_mac_)) {
            last_discovery_ms_ =
                static_cast<std::uint32_t>(event.received_time_us / 1000U);
        }
        return;
    }

    if (!peer_known_ || !radio::mac_equal(event.source, peer_mac_)) {
        return;
    }

    const std::uint32_t event_received_ms =
        static_cast<std::uint32_t>(event.received_time_us / 1000U);
    last_valid_rx_ms_ = event_received_ms;

    switch (message.type) {
        case protocol::MessageType::Ping:
            ++rx_ping_count_;
            send_ack(
                message.sequence,
                event.has_rssi
                    ? event.rssi
                    : protocol::kRssiUnavailable);
            break;

        case protocol::MessageType::Ack:
            if (ping_pending_
                && message.reference_sequence == pending_ping_sequence_) {
                ping_pending_ = false;
                ++ack_count_;

                if (event.received_time_us >= pending_ping_started_us_) {
                    last_rtt_ms_ = static_cast<std::int32_t>(
                        (event.received_time_us - pending_ping_started_us_)
                        / 1000U);
                } else {
                    last_rtt_ms_ = -1;
                }

                matching_ack_rssi_valid_ = event.has_rssi;
                if (event.has_rssi) {
                    matching_ack_rssi_ = event.rssi;
                }

                peer_ping_rssi_valid_ =
                    message.reported_rssi != protocol::kRssiUnavailable;
                if (peer_ping_rssi_valid_) {
                    peer_ping_rssi_ = message.reported_rssi;
                }
            }
            break;

        case protocol::MessageType::Hello:
            hello_received_ = true;
            hello_sequence_ = message.sequence;
            hello_rssi_ = event.rssi;
            hello_rssi_valid_ = event.has_rssi;
            hello_mode_ = from_wire_mode(message.mode);
            hello_received_ms_ = event_received_ms;
            board_.tone(2600.0F, 80);
            break;

        case protocol::MessageType::Discovery:
        default:
            break;
    }
}

void RadioLabApp::process_tx(const radio::TxEvent& event)
{
    (void)event.destination;
    latest_mac_tx_result_valid_ = true;
    latest_mac_tx_success_ = event.success;
}

void RadioLabApp::process_timers(std::uint32_t now_ms)
{
    if (now_ms - last_discovery_tx_ms_ >= kDiscoveryIntervalMs) {
        send_discovery();
        last_discovery_tx_ms_ = now_ms;
    }

    if (peer_known_) {
        const bool discovery_stale =
            now_ms - last_discovery_ms_ > kPeerLostMs;
        const bool traffic_stale =
            last_valid_rx_ms_ == 0
            || now_ms - last_valid_rx_ms_ > kPeerLostMs;

        if (discovery_stale && traffic_stale) {
            clear_active_peer();
        }
    }

    if (ping_pending_
        && now_us() - pending_ping_started_us_
            > static_cast<std::uint64_t>(kPingTimeoutMs) * 1000U) {
        ping_pending_ = false;
        ++failed_ping_count_;
    }

    if (live_enabled_
        && peer_known_
        && !ping_pending_
        && (last_live_ping_ms_ == 0
            || now_ms - last_live_ping_ms_ >= kLivePingIntervalMs)) {
        send_ping();
        last_live_ping_ms_ = now_ms;
    }
}

void RadioLabApp::send_discovery()
{
    protocol::Message message;
    message.type = protocol::MessageType::Discovery;
    message.mode = to_wire_mode(radio_.mode());
    message.sequence = next_sequence();

    std::array<std::uint8_t, protocol::kWireSize> wire{};
    if (protocol::encode(message, wire.data(), wire.size())) {
        radio_.send_broadcast(wire.data(), wire.size());
    }
}

void RadioLabApp::send_ping()
{
    if (!peer_known_ || ping_pending_) {
        return;
    }

    protocol::Message message;
    message.type = protocol::MessageType::Ping;
    message.mode = to_wire_mode(radio_.mode());
    message.sequence = next_sequence();

    std::array<std::uint8_t, protocol::kWireSize> wire{};
    if (!protocol::encode(message, wire.data(), wire.size())) {
        return;
    }

    const std::uint64_t ping_send_time_us = now_us();
    if (!radio_.send_peer(wire.data(), wire.size())) {
        ++failed_ping_count_;
        return;
    }

    ++tx_ping_count_;
    ping_pending_ = true;
    pending_ping_sequence_ = message.sequence;
    pending_ping_started_us_ = ping_send_time_us;
}

void RadioLabApp::send_ack(
    std::uint32_t reference_sequence,
    std::int8_t measured_rssi)
{
    if (!peer_known_) {
        return;
    }

    protocol::Message message;
    message.type = protocol::MessageType::Ack;
    message.mode = to_wire_mode(radio_.mode());
    message.sequence = next_sequence();
    message.reference_sequence = reference_sequence;
    message.reported_rssi = measured_rssi;

    std::array<std::uint8_t, protocol::kWireSize> wire{};
    if (protocol::encode(message, wire.data(), wire.size())) {
        radio_.send_peer(wire.data(), wire.size());
    }
}

void RadioLabApp::send_hello()
{
    if (!peer_known_) {
        return;
    }

    protocol::Message message;
    message.type = protocol::MessageType::Hello;
    message.mode = to_wire_mode(radio_.mode());
    message.sequence = next_sequence();

    std::array<std::uint8_t, protocol::kWireSize> wire{};
    if (protocol::encode(message, wire.data(), wire.size())) {
        radio_.send_peer(wire.data(), wire.size());
    }
}

void RadioLabApp::toggle_mode()
{
    const radio::Mode next_mode =
        radio_.mode() == radio::Mode::Normal
            ? radio::Mode::Lr
            : radio::Mode::Normal;

    if (!radio_.set_mode(next_mode)) {
        return;
    }

    clear_active_peer();
    send_discovery();
    last_discovery_tx_ms_ = now_ms();
}

void RadioLabApp::clear_active_peer()
{
    radio_.clear_peer();
    peer_known_ = false;
    peer_mac_.fill(0);
    last_discovery_ms_ = 0;
    last_valid_rx_ms_ = 0;
    ping_pending_ = false;
    matching_ack_rssi_valid_ = false;
    peer_ping_rssi_valid_ = false;
    last_rtt_ms_ = -1;
    latest_mac_tx_result_valid_ = false;
}

void RadioLabApp::render(std::uint32_t now_ms)
{
    const board::PowerStatus power = board_.power_status();
    const esp_app_desc_t* app = esp_app_get_description();

    char self_mac[18]{};
    format_mac(radio_.self_mac(), self_mac, sizeof(self_mac));

    char ack_rssi[12] = "--";
    if (matching_ack_rssi_valid_) {
        std::snprintf(
            ack_rssi,
            sizeof(ack_rssi),
            "%d",
            matching_ack_rssi_);
    }

    char peer_ping_rssi[12] = "--";
    if (peer_ping_rssi_valid_) {
        std::snprintf(
            peer_ping_rssi,
            sizeof(peer_ping_rssi),
            "%d",
            peer_ping_rssi_);
    }

    char rtt[12] = "--";
    if (last_rtt_ms_ >= 0) {
        std::snprintf(rtt, sizeof(rtt), "%" PRId32, last_rtt_ms_);
    }

    char battery[32] = "unavailable";
    if (power.voltage_mv >= 0) {
        if (power.level_percent >= 0) {
            std::snprintf(
                battery,
                sizeof(battery),
                "%dmV %ld%% %s",
                power.voltage_mv,
                static_cast<long>(power.level_percent),
                board::charge_state_name(power.charge_state));
        } else {
            std::snprintf(
                battery,
                sizeof(battery),
                "%dmV %s",
                power.voltage_mv,
                board::charge_state_name(power.charge_state));
        }
    }

    char hello[64] = "none";
    if (hello_received_) {
        const std::uint32_t age_seconds =
            (now_ms - hello_received_ms_) / 1000U;
        if (hello_rssi_valid_) {
            std::snprintf(
                hello,
                sizeof(hello),
                "#%" PRIu32 " %ddBm %s %" PRIu32 "s",
                hello_sequence_,
                hello_rssi_,
                radio::mode_name(hello_mode_),
                age_seconds);
        } else {
            std::snprintf(
                hello,
                sizeof(hello),
                "#%" PRIu32 " -- %s %" PRIu32 "s",
                hello_sequence_,
                radio::mode_name(hello_mode_),
                age_seconds);
        }
    }

    const char* mac_tx =
        latest_mac_tx_result_valid_
            ? (latest_mac_tx_success_ ? "OK" : "FAIL")
            : "--";

    char body[768]{};
    std::snprintf(
        body,
        sizeof(body),
        "PEER:%s  MODE:%s\n"
        "CH:%u ACT:%s LIVE:%s\n"
        "ACK RSSI:%s RTT:%sms\n"
        "PING RSSI@PEER:%s\n"
        "PING TX:%" PRIu32 " RX:%" PRIu32 " ACK:%" PRIu32 " FAIL:%" PRIu32 "\n"
        "MAC LAST:%s QDROP:%" PRIu32 "\n"
        "BAT:%s\n"
        "ID:%s\n"
        "FW:%s\n"
        "HELLO:%s\n"
        "A:run B:next B-hold:back",
        reachability_name(now_ms),
        radio::mode_name(radio_.mode()),
        static_cast<unsigned>(radio_.channel()),
        action_name(),
        live_enabled_ ? "ON" : "OFF",
        ack_rssi,
        rtt,
        peer_ping_rssi,
        tx_ping_count_,
        rx_ping_count_,
        ack_count_,
        failed_ping_count_,
        mac_tx,
        radio_.dropped_event_count(),
        battery,
        self_mac,
        app != nullptr ? app->version : "unknown",
        hello);

    board_.draw_screen("RADIO LAB", body);
    last_render_ms_ = now_ms;
}

RadioLabApp::Reachability RadioLabApp::reachability(
    std::uint32_t now_ms) const
{
    if (!peer_known_) {
        return Reachability::Lost;
    }

    if (last_valid_rx_ms_ == 0) {
        return Reachability::Found;
    }

    const std::uint32_t age = now_ms - last_valid_rx_ms_;
    if (age <= kReachableMs) {
        return Reachability::Reachable;
    }
    if (age <= kStaleMs) {
        return Reachability::Stale;
    }
    return Reachability::Found;
}

std::uint32_t RadioLabApp::next_sequence()
{
    return sequence_++;
}

std::uint32_t RadioLabApp::now_ms() const
{
    return static_cast<std::uint32_t>(now_us() / 1000U);
}

std::uint64_t RadioLabApp::now_us() const
{
    return static_cast<std::uint64_t>(esp_timer_get_time());
}

const char* RadioLabApp::action_name() const
{
    switch (action_) {
        case Action::Ping:
            return "PING";
        case Action::Hello:
            return "HELLO";
        case Action::Live:
            return "LIVE";
        case Action::Mode:
            return "MODE";
        case Action::Count:
        default:
            return "?";
    }
}

const char* RadioLabApp::reachability_name(std::uint32_t now_ms) const
{
    switch (reachability(now_ms)) {
        case Reachability::Found:
            return "FOUND";
        case Reachability::Reachable:
            return "REACH";
        case Reachability::Stale:
            return "STALE";
        case Reachability::Lost:
        default:
            return "LOST";
    }
}

}  // namespace nikos::radiolab
