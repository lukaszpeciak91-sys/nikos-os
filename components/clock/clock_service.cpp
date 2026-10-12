#include "clock/clock_service.hpp"

#include <cstdio>

namespace {

constexpr std::uint8_t kMonthOffsets[12] = {
    0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U,
};

constexpr bool date_trust_predicate(
    bool marker_configured,
    bool rtc_available,
    bool voltage_low,
    bool hardware_date_valid)
{
    return marker_configured
        && rtc_available
        && !voltage_low
        && hardware_date_valid;
}

constexpr bool leap_year_value(std::uint16_t year)
{
    return (year % 4U == 0U)
        && ((year % 100U != 0U) || (year % 400U == 0U));
}

constexpr std::uint8_t days_in_month_value(
    std::uint16_t year,
    std::uint8_t month)
{
    switch (month) {
        case 2:
            return leap_year_value(year) ? 29U : 28U;
        case 4:
        case 6:
        case 9:
        case 11:
            return 30U;
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            return 31U;
        default:
            return 0U;
    }
}

constexpr std::uint8_t clamped_day_value(
    std::uint16_t year,
    std::uint8_t month,
    std::uint8_t day)
{
    const std::uint8_t max_day =
        days_in_month_value(year, month);
    if (max_day == 0U) {
        return 1U;
    }
    return day > max_day ? max_day : (day == 0U ? 1U : day);
}

constexpr bool valid_date_value(
    std::uint16_t year,
    std::uint8_t month,
    std::uint8_t day)
{
    const std::uint8_t max_day =
        days_in_month_value(year, month);
    return year >= 2000U
        && year <= 2099U
        && max_day > 0U
        && day >= 1U
        && day <= max_day;
}

// Gregorian weekday, returned as 0=Monday ... 6=Sunday.
constexpr std::uint8_t weekday_monday_value(
    std::uint16_t year,
    std::uint8_t month,
    std::uint8_t day)
{
    std::uint32_t adjusted_year = year;
    if (month < 3U) {
        --adjusted_year;
    }

    const std::uint8_t sunday0 =
        static_cast<std::uint8_t>(
            (adjusted_year
                + adjusted_year / 4U
                - adjusted_year / 100U
                + adjusted_year / 400U
                + kMonthOffsets[month - 1U]
                + day)
            % 7U);

    return static_cast<std::uint8_t>((sunday0 + 6U) % 7U);
}

static_assert(leap_year_value(2000U));
static_assert(leap_year_value(2024U));
static_assert(!leap_year_value(2025U));
static_assert(valid_date_value(2024U, 2U, 29U));
static_assert(!valid_date_value(2025U, 2U, 29U));
static_assert(days_in_month_value(2026U, 4U) == 30U);
static_assert(days_in_month_value(2026U, 1U) == 31U);
static_assert(days_in_month_value(2024U, 2U) == 29U);
static_assert(days_in_month_value(2025U, 2U) == 28U);
static_assert(clamped_day_value(2025U, 2U, 31U) == 28U);
static_assert(clamped_day_value(2024U, 2U, 31U) == 29U);
static_assert(clamped_day_value(2025U, 2U, 29U) == 28U);
static_assert(weekday_monday_value(2026U, 10U, 11U) == 6U);
static_assert(weekday_monday_value(2026U, 10U, 1U) == 3U);
static_assert(weekday_monday_value(2000U, 1U, 1U) == 5U);
static_assert(!date_trust_predicate(false, true, false, true));
static_assert(date_trust_predicate(true, true, false, true));
static_assert(!date_trust_predicate(true, true, true, true));
static_assert(!date_trust_predicate(true, true, false, false));

}  // namespace

namespace nikos::clock {

ClockService::ClockService(
    board::Board& board,
    storage::RtcDateConfiguredStore& date_store)
    : board_(board),
      date_store_(date_store)
{
}

void ClockService::initialize_date_trust()
{
    date_trust_initialized_ = true;
    date_configured_ = false;
    marker_known_false_ = false;

    if (!board_.rtc_available()) {
        return;
    }

    if (board_.rtc_voltage_low()) {
        marker_known_false_ = date_store_.save_configured(false);
        return;
    }

    bool configured = false;
    if (!date_store_.load_configured(configured)) {
        return;
    }

    date_configured_ = configured;
    marker_known_false_ = !configured;
}

Reading ClockService::read() const
{
    Reading reading;

    if (!board_.rtc_available()) {
        return reading;
    }

    if (board_.rtc_voltage_low()) {
        (void)invalidate_date_trust(false);
        return reading;
    }

    board::RtcTime rtc_time;
    if (!board_.read_rtc_time(rtc_time)
        || rtc_time.hour > 23
        || rtc_time.minute > 59
        || rtc_time.second > 59) {
        return reading;
    }

    reading.valid = true;
    reading.time.hour = rtc_time.hour;
    reading.time.minute = rtc_time.minute;
    return reading;
}

bool ClockService::set_time(
    std::uint8_t hour,
    std::uint8_t minute) const
{
    if (hour > 23 || minute > 59 || !board_.rtc_available()) {
        return false;
    }

    if (board_.rtc_voltage_low()) {
        // PCF8563 VL lives in the seconds register. setTime() replaces that
        // register, so invalidate date trust before the time write can clear VL.
        (void)invalidate_date_trust(true);
    }

    const board::RtcTime requested{
        hour,
        minute,
        0,
    };

    if (!board_.write_rtc_time(requested)) {
        return false;
    }

    board::RtcTime verified_time;
    if (board_.rtc_voltage_low()
        || !board_.read_rtc_time(verified_time)) {
        return false;
    }

    return verified_time.hour == hour
        && verified_time.minute == minute
        && verified_time.second <= 1U;
}

DateReading ClockService::read_date() const
{
    DateReading reading;

    if (!date_trust_initialized_
        || !date_configured_
        || !board_.rtc_available()) {
        return reading;
    }

    if (board_.rtc_voltage_low()) {
        (void)invalidate_date_trust(false);
        return reading;
    }

    board::RtcDate rtc_date;
    if (!board_.read_rtc_date(rtc_date)) {
        return reading;
    }

    const Date date{
        rtc_date.year,
        rtc_date.month,
        rtc_date.day,
    };
    if (!valid_date(date)) {
        return reading;
    }

    reading.valid = true;
    reading.date = date;
    return reading;
}

DateSetResult ClockService::set_date(const Date& date) const
{
    if (!date_trust_initialized_
        || !valid_date(date)
        || !board_.rtc_available()) {
        return DateSetResult::Failed;
    }

    // Date-only setDate() does not touch the shared PCF8563 VL bit.
    // Require trustworthy time/integrity first rather than silently rewriting
    // HH:MM:SS just to clear VL.
    if (!read().valid) {
        return DateSetResult::TimeUntrusted;
    }

    // Persist false first so a later partial failure cannot leave an older
    // configured=true marker authorizing a hardware date we failed to commit
    // as trusted.
    date_configured_ = false;
    marker_known_false_ = date_store_.save_configured(false);
    if (!marker_known_false_) {
        return DateSetResult::Failed;
    }

    const board::RtcDate requested{
        date.year,
        date.month,
        date.day,
    };
    if (!board_.write_rtc_date(requested)) {
        return DateSetResult::Failed;
    }

    board::RtcDate verified;
    if (board_.rtc_voltage_low()
        || !board_.read_rtc_date(verified)
        || verified.year != date.year
        || verified.month != date.month
        || verified.day != date.day) {
        return board_.rtc_voltage_low()
            ? DateSetResult::TimeUntrusted
            : DateSetResult::Failed;
    }

    if (!date_store_.save_configured(true)) {
        // The pre-write false marker remains authoritative.
        date_configured_ = false;
        marker_known_false_ = true;
        return DateSetResult::Failed;
    }

    date_configured_ = true;
    marker_known_false_ = false;
    return DateSetResult::Success;
}

bool ClockService::invalidate_date_trust(bool force_persist) const
{
    const bool was_configured = date_configured_;
    date_configured_ = false;

    if (!date_trust_initialized_) {
        marker_known_false_ = false;
        return false;
    }

    if (!force_persist && !was_configured) {
        return marker_known_false_;
    }

    marker_known_false_ = date_store_.save_configured(false);
    return marker_known_false_;
}

bool ClockService::is_leap_year(std::uint16_t year)
{
    return leap_year_value(year);
}

std::uint8_t ClockService::days_in_month(
    std::uint16_t year,
    std::uint8_t month)
{
    return days_in_month_value(year, month);
}

bool ClockService::valid_date(const Date& date)
{
    return valid_date_value(
        date.year,
        date.month,
        date.day);
}

std::uint8_t ClockService::weekday_monday_index(
    const Date& date)
{
    return valid_date(date)
        ? weekday_monday_value(
            date.year,
            date.month,
            date.day)
        : 0U;
}

void ClockService::format_hhmm(
    const Reading& reading,
    char* output,
    std::size_t output_size)
{
    if (output == nullptr || output_size == 0) {
        return;
    }

    if (!reading.valid) {
        std::snprintf(output, output_size, "--:--");
        return;
    }

    std::snprintf(
        output,
        output_size,
        "%02u:%02u",
        static_cast<unsigned>(reading.time.hour),
        static_cast<unsigned>(reading.time.minute));
}

void ClockService::format_ddmmyyyy(
    const DateReading& reading,
    char* output,
    std::size_t output_size)
{
    if (output == nullptr || output_size == 0) {
        return;
    }

    if (!reading.valid) {
        std::snprintf(output, output_size, "--.--.----");
        return;
    }

    std::snprintf(
        output,
        output_size,
        "%02u.%02u.%04u",
        static_cast<unsigned>(reading.date.day),
        static_cast<unsigned>(reading.date.month),
        static_cast<unsigned>(reading.date.year));
}

}  // namespace nikos::clock
