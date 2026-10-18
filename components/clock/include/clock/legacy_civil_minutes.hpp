#pragma once
#include <cstdint>
#include "clock/warsaw_time_rules.hpp"

// ENT1 encoded Polish local minutes since 2000-01-01. This narrow
// compatibility function deliberately refuses ambiguous / nonexistent hours.
namespace nikos::clock {
constexpr bool legacy_local_minutes_to_utc(
    std::uint32_t legacy_minutes, std::uint32_t& utc_minutes)
{
    warsaw::DateTime local{};
    if (!warsaw::from_seconds(
            static_cast<std::int64_t>(legacy_minutes)*60,local))
        return false;
    const auto converted=warsaw::local_to_utc(local);
    if (converted.status!=warsaw::LocalStatus::Unique) return false;
    const auto utc=warsaw::valid(converted.daylight_utc)
        ? converted.daylight_utc : converted.standard_utc;
    utc_minutes=static_cast<std::uint32_t>(warsaw::seconds(utc)/60);
    return true;
}
} // namespace nikos::clock
