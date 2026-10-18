#pragma once
#include <cstdint>
#include "clock/warsaw_time_rules.hpp"

// ENT1 encoded Polish LOCAL civil minutes since 2000-01-01.
// Only Guard recovery uses conservative DST-boundary resolution here;
// RTC migration and manual setting MUST reject ambiguous local timestamps.
namespace nikos::clock {
enum class LegacyMinuteStatus : std::uint8_t {
    Invalid, Exact, ConservativeBoundary
};
constexpr LegacyMinuteStatus legacy_local_minutes_to_utc(
    std::uint32_t legacy_minutes, std::uint32_t& utc_minutes)
{
    warsaw::DateTime local{};
    if (!warsaw::from_seconds(
            static_cast<std::int64_t>(legacy_minutes)*60,local))
        return LegacyMinuteStatus::Invalid;
    const auto converted=warsaw::local_to_utc(local);
    warsaw::DateTime utc{};
    LegacyMinuteStatus result=LegacyMinuteStatus::Exact;
    if (converted.status==warsaw::LocalStatus::Unique) {
        utc=warsaw::valid(converted.daylight_utc)
            ? converted.daylight_utc : converted.standard_utc;
    } else if (converted.status==warsaw::LocalStatus::Ambiguous) {
        // Fall-back overlap: later possible UTC start conservatively
        // avoids early unlock without guessing the historical occurrence.
        utc=converted.standard_utc;
        result=LegacyMinuteStatus::ConservativeBoundary;
    } else if (converted.status==warsaw::LocalStatus::Nonexistent) {
        // Legacy firmware lacked DST, so its local clock could show 02:xx
        // in the skipped spring hour. Use the later possible UTC start.
        if (!warsaw::from_seconds(warsaw::seconds(local)-3600,utc))
            return LegacyMinuteStatus::Invalid;
        result=LegacyMinuteStatus::ConservativeBoundary;
    } else {
        return LegacyMinuteStatus::Invalid;
    }
    utc_minutes=static_cast<std::uint32_t>(warsaw::seconds(utc)/60);
    return result;
}
} // namespace nikos::clock
