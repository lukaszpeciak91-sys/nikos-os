#include "radiolab/radiolab_app.hpp"

#include <array>
#include <cstdio>

#include "esp_log.h"
#include "esp_timer.h"
#include "protocol/protocol.hpp"

namespace {

constexpr char kTag[] = "radiolab";

constexpr std::uint32_t kDiscoveryIntervalMs = 2000;
constexpr std::uint32_t kPingTimeoutMs = 1200;
constexpr std::uint32_t kPeerLostMs = 7000;
constexpr std::uint32_t kLinkFreshMs = 4000;
constexpr std::uint32_t kBatterySampleIntervalMs = 1000;

constexpr std::int16_t kIndicatorX = 24;
constexpr std::int16_t kIndicatorY = 38;
constexpr std::int16_t kIndicatorRadius = 17;

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
    update_battery_sample(now);
    show_main_screen(now);
}

void RadioLabApp::update()
{
    const std::uint32_t input_now = now_ms();

    process_input(board_.poll_input(), input_now);
    process_radio_events();

    const std::uint32_t current_now = now_ms();
    process_timers(current_now);
    update_battery_sample(current_now);

    if (!hello_screen_active_) {
        render_main_if_changed(current_now);
    }
}

void RadioLabApp::process_input(
    const board::InputState& input,
    std::uint32_t now_ms)
{
    const bool any_button_event =
        input.a_short || input.a_long || input.b_short || input.b_long;

    if (hello_screen_active_) {
        if (any_button_event) {
            hello_screen_active_ = false;
            show_main_screen(now_ms);
        }
        return;
    }

    if (input.b_long) {
        toggle_mode();
        return;
    }

    if (input.a_short) {
        send_ping();
    }

    if (input.b_short) {
        send_hello();
    }
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
            record_peer_rx(event);
        }
        return;
    }

    if (!peer_known_ || !radio::mac_equal(event.source, peer_mac_)) {
        return;
    }

    const std::uint32_t event_received_ms =
        static_cast<std::uint32_t>(event.received_time_us / 1000U);
    last_valid_rx_ms_ = event_received_ms;
    record_peer_rx(event);

    switch (message.type) {
        case protocol::MessageType::Ping:
            ++rx_ping_count_;
            board_.tone(2200.0F, 60);
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
            board_.tone(2600.0F, 90);
            show_hello_screen();
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
            last_discovery_ms_ == 0
            || now_ms - last_discovery_ms_ > kPeerLostMs;
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
}

void RadioLabApp::record_peer_rx(const radio::RxEvent& event)
{
    latest_peer_rx_seen_ = true;
    latest_peer_rx_ms_ =
        static_cast<std::uint32_t>(event.received_time_us / 1000U);
    latest_peer_rx_rssi_valid_ = event.has_rssi;

    if (event.has_rssi) {
        latest_peer_rx_rssi_ = event.rssi;
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
    latest_peer_rx_seen_ = false;
    latest_peer_rx_ms_ = 0;
    latest_peer_rx_rssi_valid_ = false;
    ping_pending_ = false;
    matching_ack_rssi_valid_ = false;
    peer_ping_rssi_valid_ = false;
    last_rtt_ms_ = -1;
    latest_mac_tx_result_valid_ = false;
}

bool RadioLabApp::link_is_fresh(std::uint32_t now_ms) const
{
    return peer_known_
        && latest_peer_rx_seen_
        && now_ms - latest_peer_rx_ms_ <= kLinkFreshMs;
}

void RadioLabApp::update_battery_sample(std::uint32_t now_ms)
{
    if (battery_sample_valid_
        && now_ms - last_battery_sample_ms_ < kBatterySampleIntervalMs) {
        return;
    }

    const board::PowerStatus power = board_.power_status();
    cached_battery_percent_ =
        power.level_percent >= 0 ? power.level_percent : -1;
    last_battery_sample_ms_ = now_ms;
    battery_sample_valid_ = true;
}

void RadioLabApp::show_main_screen(std::uint32_t now_ms)
{
    board_.clear_screen();
    board_.draw_text_region(10, 108, 90, 22, "A PING", 2);
    board_.draw_text_region(130, 108, 100, 22, "B HELLO", 2);

    main_render_state_valid_ = false;
    render_main_if_changed(now_ms);
}

void RadioLabApp::render_main_if_changed(std::uint32_t now_ms)
{
    const bool fresh = link_is_fresh(now_ms);
    const bool rssi_valid = fresh && latest_peer_rx_rssi_valid_;
    const std::int16_t rssi = latest_peer_rx_rssi_;

    const std::int32_t battery_percent = cached_battery_percent_;
    const radio::Mode mode = radio_.mode();

    if (!main_render_state_valid_ || fresh != rendered_link_fresh_) {
        board_.fill_circle(
            kIndicatorX,
            kIndicatorY,
            kIndicatorRadius,
            fresh ? board::DisplayColor::Green : board::DisplayColor::Red);
    }

    if (!main_render_state_valid_
        || rssi_valid != rendered_rssi_valid_
        || (rssi_valid && rssi != rendered_rssi_)) {
        char rssi_text[24]{};
        if (rssi_valid) {
            std::snprintf(rssi_text, sizeof(rssi_text), "RSSI: %d", rssi);
        } else {
            std::snprintf(rssi_text, sizeof(rssi_text), "RSSI: --");
        }

        board_.draw_text_region(50, 23, 185, 34, rssi_text, 3);
    }

    if (!main_render_state_valid_
        || battery_percent != rendered_battery_percent_) {
        char battery_text[20]{};
        if (battery_percent >= 0) {
            std::snprintf(
                battery_text,
                sizeof(battery_text),
                "BAT %ld%%",
                static_cast<long>(battery_percent));
        } else {
            std::snprintf(battery_text, sizeof(battery_text), "BAT --%%");
        }

        board_.draw_text_region(8, 72, 100, 20, battery_text, 2);
    }

    if (!main_render_state_valid_ || mode != rendered_mode_) {
        board_.draw_text_region(
            182,
            4,
            54,
            12,
            radio::mode_name(mode),
            1);
    }

    rendered_link_fresh_ = fresh;
    rendered_rssi_valid_ = rssi_valid;
    rendered_rssi_ = rssi;
    rendered_battery_percent_ = battery_percent;
    rendered_mode_ = mode;
    main_render_state_valid_ = true;
}

void RadioLabApp::show_hello_screen()
{
    if (hello_screen_active_) {
        return;
    }

    hello_screen_active_ = true;
    board_.clear_screen();
    board_.draw_text_region(58, 47, 130, 38, "HELLO", 4);
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

}  // namespace nikos::radiolab
