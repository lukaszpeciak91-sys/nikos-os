#pragma once

#include <cstdint>

#include "board/board.hpp"
#include "radio/radio.hpp"

namespace nikos::radiolab {

class RadioLabApp final {
public:
    enum class UpdateResult : std::uint8_t {
        Continue,
        ExitRequested,
    };

    RadioLabApp(board::Board& board, radio::RadioService& radio);

    void begin();
    void end();
    void redraw();
    UpdateResult update(
        const board::InputState& input,
        bool render_enabled = true);

private:
    enum class DeliveryFeedback : std::uint8_t {
        None,
        PingOk,
        HelloOk,
    };

    void process_input(const board::InputState& input, std::uint32_t now_ms);
    void process_radio_events();
    void process_rx(const radio::RxEvent& event);
    void process_tx(const radio::TxEvent& event);
    void process_timers(std::uint32_t now_ms);

    void record_peer_rx(const radio::RxEvent& event);
    void send_discovery();
    void send_ping();
    void send_ack(std::uint32_t reference_sequence, std::int8_t measured_rssi);
    void send_hello();
    void toggle_mode();
    void clear_active_peer();
    void reset_session_state();

    bool link_is_fresh(std::uint32_t now_ms) const;
    void update_battery_sample(std::uint32_t now_ms, bool force = false);
    void show_main_screen(std::uint32_t now_ms);
    void render_main_if_changed(std::uint32_t now_ms);
    void render_action_area_if_changed(std::uint32_t now_ms);
    void show_delivery_feedback(DeliveryFeedback feedback, std::uint32_t now_ms);
    void show_hello_screen();

    std::uint32_t next_sequence();
    std::uint32_t now_ms() const;
    std::uint64_t now_us() const;

    board::Board& board_;
    radio::RadioService& radio_;

    std::uint32_t sequence_ = 1;

    bool peer_known_ = false;
    radio::MacAddress peer_mac_{};
    std::uint32_t last_discovery_ms_ = 0;
    std::uint32_t last_valid_rx_ms_ = 0;
    std::uint32_t last_discovery_tx_ms_ = 0;

    bool latest_peer_rx_seen_ = false;
    std::uint32_t latest_peer_rx_ms_ = 0;
    std::int16_t latest_peer_rx_rssi_ = 0;
    bool latest_peer_rx_rssi_valid_ = false;

    bool ping_pending_ = false;
    std::uint32_t pending_ping_sequence_ = 0;
    std::uint64_t pending_ping_started_us_ = 0;

    bool hello_pending_ = false;
    std::uint32_t pending_hello_sequence_ = 0;
    std::uint64_t pending_hello_started_us_ = 0;

    std::int16_t matching_ack_rssi_ = 0;
    bool matching_ack_rssi_valid_ = false;
    std::int16_t peer_ping_rssi_ = 0;
    bool peer_ping_rssi_valid_ = false;
    std::int32_t last_rtt_ms_ = -1;

    std::uint32_t tx_ping_count_ = 0;
    std::uint32_t rx_ping_count_ = 0;
    std::uint32_t ack_count_ = 0;
    std::uint32_t failed_ping_count_ = 0;

    bool latest_mac_tx_result_valid_ = false;
    bool latest_mac_tx_success_ = false;

    bool hello_received_ = false;
    std::uint32_t hello_sequence_ = 0;
    std::int16_t hello_rssi_ = 0;
    bool hello_rssi_valid_ = false;
    radio::Mode hello_mode_ = radio::Mode::Normal;
    std::uint32_t hello_received_ms_ = 0;
    bool hello_screen_active_ = false;

    bool battery_sample_valid_ = false;
    std::uint32_t last_battery_sample_ms_ = 0;
    std::int32_t cached_battery_percent_ = -1;

    DeliveryFeedback delivery_feedback_ = DeliveryFeedback::None;
    std::uint32_t delivery_feedback_started_ms_ = 0;
    bool action_area_render_valid_ = false;
    DeliveryFeedback rendered_delivery_feedback_ = DeliveryFeedback::None;

    bool render_enabled_ = true;
    bool main_render_state_valid_ = false;
    bool rendered_link_fresh_ = false;
    bool rendered_rssi_valid_ = false;
    std::int16_t rendered_rssi_ = 0;
    std::int32_t rendered_battery_percent_ = -2;
    radio::Mode rendered_mode_ = radio::Mode::Normal;
    bool exit_requested_ = false;
};

}  // namespace nikos::radiolab
