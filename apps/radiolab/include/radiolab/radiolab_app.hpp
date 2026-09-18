#pragma once

#include <cstdint>

#include "board/board.hpp"
#include "radio/radio.hpp"

namespace nikos::radiolab {

class RadioLabApp final {
public:
    RadioLabApp(board::Board& board, radio::RadioService& radio);

    void begin();
    void update();

private:
    enum class Action : std::uint8_t {
        Ping,
        Hello,
        Live,
        Mode,
        Count,
    };

    enum class Reachability : std::uint8_t {
        Lost,
        Found,
        Reachable,
        Stale,
    };

    void process_input(const board::InputState& input, std::uint32_t now_ms);
    void process_radio_events();
    void process_rx(const radio::RxEvent& event);
    void process_tx(const radio::TxEvent& event);
    void process_timers(std::uint32_t now_ms);

    void send_discovery();
    void send_ping();
    void send_ack(std::uint32_t reference_sequence, std::int8_t measured_rssi);
    void send_hello();
    void toggle_mode();
    void clear_active_peer();

    void render(std::uint32_t now_ms);
    Reachability reachability(std::uint32_t now_ms) const;

    std::uint32_t next_sequence();
    std::uint32_t now_ms() const;
    std::uint64_t now_us() const;
    const char* action_name() const;
    const char* reachability_name(std::uint32_t now_ms) const;

    board::Board& board_;
    radio::RadioService& radio_;

    Action action_ = Action::Ping;
    std::uint32_t sequence_ = 1;

    bool peer_known_ = false;
    radio::MacAddress peer_mac_{};
    std::uint32_t last_discovery_ms_ = 0;
    std::uint32_t last_valid_rx_ms_ = 0;

    bool live_enabled_ = false;
    std::uint32_t last_live_ping_ms_ = 0;
    std::uint32_t last_discovery_tx_ms_ = 0;

    bool ping_pending_ = false;
    std::uint32_t pending_ping_sequence_ = 0;
    std::uint64_t pending_ping_started_us_ = 0;

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

    std::uint32_t last_render_ms_ = 0;
};

}  // namespace nikos::radiolab
