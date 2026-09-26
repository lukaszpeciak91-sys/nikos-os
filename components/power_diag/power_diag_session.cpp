#include "power_diag/power_diag_session.hpp"

namespace nikos::power_diag {

void PowerDiagSession::start(
    std::uint64_t now_us,
    const Observation& observation)
{
    state_ = SessionState::Running;
    started_at_us_ = now_us;
    last_observation_us_ = now_us;
    last_observation_ = observation;

    total_us_ = 0;
    lcd_active_us_ = 0;
    lcd_dimmed_us_ = 0;
    lcd_off_us_ = 0;
    communicator_on_us_ = 0;
    communicator_ui_us_ = 0;
    radiolab_us_ = 0;

    reset_session_battery_from_cached_sample();
}

void PowerDiagSession::observe(
    std::uint64_t now_us,
    const Observation& observation)
{
    if (state_ != SessionState::Running) {
        last_observation_ = observation;
        return;
    }

    if (now_us >= last_observation_us_) {
        accumulate(
            now_us - last_observation_us_,
            last_observation_);
    }

    last_observation_us_ = now_us;
    last_observation_ = observation;
}

void PowerDiagSession::record_battery_sample(
    const board::PowerStatus& status)
{
    charge_state_ = status.charge_state;

    if (status.voltage_mv <= 0 || status.level_percent < 0) {
        return;
    }

    cached_battery_valid_ = true;
    cached_battery_ = status;
    battery_current_supported_ = status.current_supported;
    battery_current_ma_ = status.current_ma;

    if (state_ != SessionState::Running) {
        return;
    }

    if (status.current_supported) {
        battery_current_sum_ma_ +=
            static_cast<std::int64_t>(status.current_ma);
        ++battery_current_sample_count_;
    }

    if (start_voltage_mv_ <= 0 || start_percent_ < 0) {
        start_voltage_mv_ = status.voltage_mv;
        start_percent_ = status.level_percent;
        minimum_voltage_mv_ = status.voltage_mv;
    }

    current_voltage_mv_ = status.voltage_mv;
    current_percent_ = status.level_percent;

    if (minimum_voltage_mv_ <= 0
        || status.voltage_mv < minimum_voltage_mv_) {
        minimum_voltage_mv_ = status.voltage_mv;
    }
}

SessionState PowerDiagSession::state() const
{
    return state_;
}

bool PowerDiagSession::running() const
{
    return state_ == SessionState::Running;
}

Snapshot PowerDiagSession::snapshot() const
{
    Snapshot result;
    result.state = state_;

    result.started_at_us = started_at_us_;
    result.total_us = total_us_;
    result.lcd_active_us = lcd_active_us_;
    result.lcd_dimmed_us = lcd_dimmed_us_;
    result.lcd_off_us = lcd_off_us_;
    result.communicator_on_us = communicator_on_us_;
    result.communicator_ui_us = communicator_ui_us_;
    result.radiolab_us = radiolab_us_;

    result.start_voltage_mv = start_voltage_mv_;
    result.start_percent = start_percent_;
    result.current_voltage_mv = current_voltage_mv_;
    result.current_percent = current_percent_;
    result.minimum_voltage_mv = minimum_voltage_mv_;
    result.delta_voltage_mv =
        start_voltage_mv_ > 0 && current_voltage_mv_ > 0
            ? static_cast<std::int32_t>(current_voltage_mv_)
                - static_cast<std::int32_t>(start_voltage_mv_)
            : 0;
    result.battery_current_ma = battery_current_ma_;
    result.battery_current_supported = battery_current_supported_;
    result.battery_current_sample_count =
        battery_current_sample_count_;
    result.battery_current_average_ma =
        battery_current_sample_count_ > 0
            ? static_cast<std::int32_t>(
                battery_current_sum_ma_
                / static_cast<std::int64_t>(
                    battery_current_sample_count_))
            : 0;
    result.charge_state = charge_state_;

    result.observation = last_observation_;
    return result;
}

void PowerDiagSession::accumulate(
    std::uint64_t elapsed_us,
    const Observation& observation)
{
    total_us_ += elapsed_us;

    switch (observation.display_state) {
        case DisplayState::Dimmed:
            lcd_dimmed_us_ += elapsed_us;
            break;
        case DisplayState::Off:
            lcd_off_us_ += elapsed_us;
            break;
        case DisplayState::Active:
        default:
            lcd_active_us_ += elapsed_us;
            break;
    }

    if (observation.communicator_enabled) {
        communicator_on_us_ += elapsed_us;
    }
    if (observation.communicator_foreground) {
        communicator_ui_us_ += elapsed_us;
    }
    if (observation.radiolab_foreground) {
        radiolab_us_ += elapsed_us;
    }
}

void PowerDiagSession::reset_session_battery_from_cached_sample()
{
    start_voltage_mv_ = -1;
    start_percent_ = -1;
    current_voltage_mv_ = -1;
    current_percent_ = -1;
    minimum_voltage_mv_ = -1;
    battery_current_ma_ = 0;
    battery_current_supported_ = false;
    battery_current_sum_ma_ = 0;
    battery_current_sample_count_ = 0;

    if (!cached_battery_valid_) {
        return;
    }

    start_voltage_mv_ = cached_battery_.voltage_mv;
    start_percent_ = cached_battery_.level_percent;
    current_voltage_mv_ = cached_battery_.voltage_mv;
    current_percent_ = cached_battery_.level_percent;
    minimum_voltage_mv_ = cached_battery_.voltage_mv;
    battery_current_ma_ = cached_battery_.current_ma;
    battery_current_supported_ = cached_battery_.current_supported;
}

}  // namespace nikos::power_diag
