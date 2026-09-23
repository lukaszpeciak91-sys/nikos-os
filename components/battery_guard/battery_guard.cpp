#include "battery_guard/battery_guard.hpp"

namespace nikos::battery_guard {

BatteryGuard::BatteryGuard(board::Board& board)
    : board_(board)
{
}

UpdateResult BatteryGuard::update(std::uint32_t now_ms)
{
    UpdateResult result;

    if (sample_valid_
        && now_ms - last_sample_ms_ < kSampleIntervalMs) {
        return result;
    }

    sample(now_ms, result);
    return result;
}

AdvisoryLevel BatteryGuard::pending_advisory() const
{
    return pending_advisory_;
}

void BatteryGuard::acknowledge_advisory()
{
    pending_advisory_ = AdvisoryLevel::None;
}

void BatteryGuard::sample(
    std::uint32_t now_ms,
    UpdateResult& result)
{
    sample_valid_ = true;
    last_sample_ms_ = now_ms;
    result.sampled = true;

    const board::PowerStatus status = board_.power_status();

    if (status.charge_state == board::ChargeState::Charging) {
        // USB/charging is a meaningful recovery boundary: advisories are
        // re-armed, pending advisory state is cleared, and critical
        // confirmation is cancelled.
        low_armed_ = true;
        very_low_armed_ = true;
        pending_advisory_ = AdvisoryLevel::None;
        critical_confirm_count_ = 0;
        return;
    }

    update_advisory(status);
    result.critical_confirmed = update_critical(status);
}

void BatteryGuard::update_advisory(
    const board::PowerStatus& status)
{
    if (status.level_percent < 0) {
        return;
    }

    if (status.level_percent > kLowRearmPercent) {
        low_armed_ = true;
    }
    if (status.level_percent > kVeryLowRearmPercent) {
        very_low_armed_ = true;
    }

    if (status.level_percent <= kVeryLowPercent
        && very_low_armed_) {
        very_low_armed_ = false;
        low_armed_ = false;
        pending_advisory_ = AdvisoryLevel::VeryLow;
        return;
    }

    if (status.level_percent <= kLowPercent
        && low_armed_) {
        low_armed_ = false;
        if (pending_advisory_ != AdvisoryLevel::VeryLow) {
            pending_advisory_ = AdvisoryLevel::Low;
        }
    }
}

bool BatteryGuard::update_critical(
    const board::PowerStatus& status)
{
    if (status.voltage_mv < 0
        || status.voltage_mv > kCriticalVoltageMv) {
        critical_confirm_count_ = 0;
        return false;
    }

    if (critical_confirm_count_ < kCriticalConfirmSamples) {
        ++critical_confirm_count_;
    }

    return critical_confirm_count_ >= kCriticalConfirmSamples;
}

}  // namespace nikos::battery_guard
