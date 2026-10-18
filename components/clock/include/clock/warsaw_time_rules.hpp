#pragma once
#include <cstdint>

// Pure, offline Europe/Warsaw conversion for the approved 2000-2099 RTC range.
// This module has no RTC, NVS, UI, runtime clock or platform dependencies.
namespace nikos::clock::warsaw {

struct DateTime {
    std::uint16_t year;
    std::uint8_t month, day, hour, minute, second;
};
enum class LocalStatus : std::uint8_t { Invalid, Unique, Nonexistent, Ambiguous };
struct ZonedTime { DateTime time{}; std::int8_t utc_offset_hours = 0; };
struct LocalResolution {
    LocalStatus status = LocalStatus::Invalid;
    // On Ambiguous: standard_utc and daylight_utc are both valid.
    DateTime standard_utc{};
    DateTime daylight_utc{};
};

constexpr bool leap(std::uint16_t y) {
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}
constexpr std::uint8_t month_days(std::uint16_t y, std::uint8_t m) {
    constexpr std::uint8_t lengths[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return m >= 1 && m <= 12
        ? static_cast<std::uint8_t>(lengths[m-1] + (m == 2 && leap(y) ? 1 : 0))
        : 0;
}
constexpr bool valid(const DateTime& t) {
    return t.year >= 2000 && t.year <= 2099 && t.month >= 1 && t.month <= 12
        && t.day >= 1 && t.day <= month_days(t.year,t.month)
        && t.hour <= 23 && t.minute <= 59 && t.second <= 59;
}
constexpr std::int64_t days_since_2000(std::uint16_t year,
                                        std::uint8_t month, std::uint8_t day) {
    std::int64_t days = 0;
    for (std::uint16_t y=2000; y<year; ++y) days += leap(y) ? 366 : 365;
    for (std::uint8_t m=1; m<month; ++m) days += month_days(year,m);
    return days + day - 1;
}
constexpr std::uint8_t last_sunday(std::uint16_t year, std::uint8_t month) {
    if (year < 2000 || year > 2099 || (month != 3 && month != 10)) return 0;
    const std::uint8_t last = month_days(year, month);
    // 2000-01-01 was Saturday (6 when Sunday=0).
    const auto weekday = static_cast<std::uint8_t>(
        (6 + days_since_2000(year, month, last)) % 7);
    return static_cast<std::uint8_t>(last - weekday);
}
constexpr std::int64_t seconds(const DateTime& t) {
    return days_since_2000(t.year,t.month,t.day)*86400
        + t.hour*3600 + t.minute*60 + t.second;
}
constexpr bool from_seconds(std::int64_t value, DateTime& out) {
    if (value < 0) return false;
    std::int64_t days = value / 86400;
    const std::int64_t remainder = value % 86400;
    std::uint16_t year = 2000;
    while (year <= 2099 && days >= (leap(year) ? 366 : 365)) {
        days -= leap(year) ? 366 : 365;
        ++year;
    }
    if (year > 2099) return false;
    std::uint8_t month = 1;
    while (month <= 12 && days >= month_days(year,month)) {
        days -= month_days(year,month);
        ++month;
    }
    out = {year,month,static_cast<std::uint8_t>(days+1),
           static_cast<std::uint8_t>(remainder/3600),
           static_cast<std::uint8_t>((remainder/60)%60),
           static_cast<std::uint8_t>(remainder%60)};
    return true;
}
constexpr bool daylight_utc(const DateTime& utc) {
    if (!valid(utc)) return false;
    const auto stamp = seconds(utc);
    const DateTime march{utc.year,3,last_sunday(utc.year,3),1,0,0};
    const DateTime october{utc.year,10,last_sunday(utc.year,10),1,0,0};
    return stamp >= seconds(march) && stamp < seconds(october);
}
constexpr bool utc_to_local(const DateTime& utc, ZonedTime& out) {
    if (!valid(utc)) return false;
    const auto offset = static_cast<std::int8_t>(daylight_utc(utc) ? 2 : 1);
    DateTime local{};
    if (!from_seconds(seconds(utc)+offset*3600,local)) return false;
    out={local,offset};
    return true;
}
constexpr LocalResolution local_to_utc(const DateTime& local) {
    LocalResolution result{};
    if (!valid(local)) return result;
    bool standard=false, daylight=false;
    DateTime utc_standard{},utc_daylight{};
    ZonedTime roundtrip{};
    if (from_seconds(seconds(local)-3600,utc_standard)
        && utc_to_local(utc_standard,roundtrip)
        && seconds(roundtrip.time)==seconds(local) && roundtrip.utc_offset_hours==1)
        standard=true;
    if (from_seconds(seconds(local)-7200,utc_daylight)
        && utc_to_local(utc_daylight,roundtrip)
        && seconds(roundtrip.time)==seconds(local) && roundtrip.utc_offset_hours==2)
        daylight=true;
    result.status = standard && daylight ? LocalStatus::Ambiguous
        : standard || daylight ? LocalStatus::Unique : LocalStatus::Nonexistent;
    if (standard) result.standard_utc=utc_standard;
    if (daylight) result.daylight_utc=utc_daylight;
    return result;
}
} // namespace nikos::clock::warsaw
