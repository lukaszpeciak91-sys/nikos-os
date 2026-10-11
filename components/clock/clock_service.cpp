#include "clock/clock_service.hpp"

#include <cstdio>

namespace {

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

}  // namespace

namespace nikos::clock {

ClockService::ClockService(board::Board& board)
    : board_(board)
{
}

Reading ClockService::read() const
{
    Reading reading;

    if (!board_.rtc_available() || board_.rtc_voltage_low()) {
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

    if (!board_.rtc_available() || board_.rtc_voltage_low()) {
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

bool ClockService::set_date(const Date& date) const
{
    if (!valid_date(date) || !board_.rtc_available()) {
        return false;
    }

    const board::RtcDate requested{
        date.year,
        date.month,
        date.day,
    };
    if (!board_.write_rtc_date(requested)) {
        return false;
    }

    board::RtcDate verified;
    if (board_.rtc_voltage_low()
        || !board_.read_rtc_date(verified)) {
        return false;
    }

    return verified.year == date.year
        && verified.month == date.month
        && verified.day == date.day;
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
