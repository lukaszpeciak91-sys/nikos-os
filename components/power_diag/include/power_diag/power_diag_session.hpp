#pragma once

#include <cstdint>

#include "board/board.hpp"

namespace nikos::power_diag {

enum class SessionState : std::uint8_t {
    Inactive,
    Running,
};

enum class DisplayState : std::uint8_t {
    Active,
    Dimmed,
    Off,
};

enum class RxProfile : std::uint8_t {
    Off,
    Foreground,
    Background,
};

enum class RadioMode : std::uint8_t {
    Normal,
    Lr,
};

struct Observation {
    DisplayState display_state = DisplayState::Active;
    bool communicator_enabled = false;
    bool communicator_foreground = false;
    bool radiolab_foreground = false;

    RxProfile rx_profile = RxProfile::Off;
    std::uint16_t rx_interval_ms = 0;
    std::uint16_t rx_wake_window_ms = 0;

    RadioMode radio_mode = RadioMode::Normal;
    bool peer_known = false;
    bool peer_reachable = false;
    bool rssi_valid = false;
    std::int8_t rssi = 0;
};

struct Snapshot {
    SessionState state = SessionState::Inactive;

    std::uint64_t total_us = 0;
    std::uint64_t lcd_active_us = 0;
    std::uint64_t lcd_dimmed_us = 0;
    std::uint64_t lcd_off_us = 0;
    std::uint64_t communicator_on_us = 0;
    std::uint64_t communicator_ui_us = 0;
    std::uint64_t radiolab_us = 0;

    std::int16_t start_voltage_mv = -1;
    std::int32_t start_percent = -1;
    std::int16_t current_voltage_mv = -1;
    std::int32_t current_percent = -1;
    std::int16_t minimum_voltage_mv = -1;
    std::int32_t delta_voltage_mv = 0;
    board::ChargeState charge_state = board::ChargeState::Unknown;

    Observation observation{};
};

class PowerDiagSession final {
public:
    void start(std::uint64_t now_us, const Observation& observation);
    void observe(std::uint64_t now_us, const Observation& observation);
    void record_battery_sample(const board::PowerStatus& status);

    SessionState state() const;
    bool running() const;
    Snapshot snapshot() const;

private:
    void accumulate(
        std::uint64_t elapsed_us,
        const Observation& observation);
    void reset_session_battery_from_cached_sample();

    SessionState state_ = SessionState::Inactive;
    std::uint64_t last_observation_us_ = 0;
    Observation last_observation_{};

    std::uint64_t total_us_ = 0;
    std::uint64_t lcd_active_us_ = 0;
    std::uint64_t lcd_dimmed_us_ = 0;
    std::uint64_t lcd_off_us_ = 0;
    std::uint64_t communicator_on_us_ = 0;
    std::uint64_t communicator_ui_us_ = 0;
    std::uint64_t radiolab_us_ = 0;

    bool cached_battery_valid_ = false;
    board::PowerStatus cached_battery_{};

    std::int16_t start_voltage_mv_ = -1;
    std::int32_t start_percent_ = -1;
    std::int16_t current_voltage_mv_ = -1;
    std::int32_t current_percent_ = -1;
    std::int16_t minimum_voltage_mv_ = -1;
    board::ChargeState charge_state_ = board::ChargeState::Unknown;
};

}  // namespace nikos::power_diag
