#include "clock/clock_service.hpp"
#include "clock/rtc_utc_commit.hpp"

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

bool ClockService::raw_rtc(warsaw::DateTime& out) const
{
    if (!board_.rtc_available()) return false;
    if (board_.rtc_voltage_low()) {
        (void)invalidate_date_trust(false);
        return false;
    }
    // Date/time registers are read separately. Reject rollover during a read.
    for (int attempt = 0; attempt < 3; ++attempt) {
        board::RtcDate before{}, after{};
        board::RtcTime time{};
        if (!board_.read_rtc_date(before) || !board_.read_rtc_time(time)
            || !board_.read_rtc_date(after)) return false;
        if (before.year != after.year || before.month != after.month
            || before.day != after.day) continue;
        const warsaw::DateTime candidate{
            before.year,before.month,before.day,time.hour,time.minute,time.second};
        if (!warsaw::valid(candidate)) return false;
        out=candidate;
        return true;
    }
    return false;
}

bool ClockService::trusted_utc(warsaw::DateTime& out) const
{
    return basis_ == storage::RtcTimeBasis::Utc
        && date_trust_initialized_ && date_configured_
        && raw_rtc(out);
}

bool ClockService::read_utc(warsaw::DateTime& out) const
{
    return trusted_utc(out);
}

bool ClockService::local_now(warsaw::DateTime& out) const
{
    warsaw::DateTime raw{};
    if (basis_ == storage::RtcTimeBasis::Utc) {
        warsaw::ZonedTime converted{};
        if (!trusted_utc(raw) || !warsaw::utc_to_local(raw,converted))
            return false;
        out=converted.time;
        return true;
    }
    if (basis_ != storage::RtcTimeBasis::LegacyLocal || !raw_rtc(raw))
        return false;
    out=raw;
    return true;
}

bool ClockService::commit_utc(const warsaw::DateTime& utc) const
{
    // The host tests exercise this exact sequence through fake operations.
    struct Operations {
        const ClockService& clock;
        bool available() const { return clock.board_.rtc_available(); }
        bool save_pending() const {
            return clock.basis_store_.save(storage::RtcTimeBasis::Pending);
        }
        void mark_pending() const {
            clock.basis_=storage::RtcTimeBasis::Pending;
            clock.date_configured_=false;
        }
        bool invalidate_date_trust() const {
            clock.marker_known_false_=clock.date_store_.save_configured(false);
            return clock.marker_known_false_;
        }
        bool write_date(const warsaw::DateTime& t) const {
            return clock.board_.write_rtc_date({t.year,t.month,t.day});
        }
        bool write_time(const warsaw::DateTime& t) const {
            return clock.board_.write_rtc_time({t.hour,t.minute,t.second});
        }
        bool readback(warsaw::DateTime& t) const { return clock.raw_rtc(t); }
        bool save_utc_basis() const {
            return clock.basis_store_.save(storage::RtcTimeBasis::Utc);
        }
        bool restore_date_trust() const {
            return clock.date_store_.save_configured(true);
        }
        void mark_trusted_utc() const {
            clock.basis_=storage::RtcTimeBasis::Utc;
            clock.date_configured_=true;
            clock.marker_known_false_=false;
            clock.staged_time_=false;
            clock.staged_date_=false;
        }
    };
    Operations operations{*this};
    return commit_utc_checked(utc,operations);
}

bool ClockService::write_local(const warsaw::DateTime& local) const
{
    const auto resolution=warsaw::local_to_utc(local);
    last_local_status_=resolution.status;
    if (resolution.status!=warsaw::LocalStatus::Unique) return false;
    // Exactly one candidate is valid for an unambiguous local instant.
    const auto utc=warsaw::valid(resolution.daylight_utc)
        ? resolution.daylight_utc : resolution.standard_utc;
    return commit_utc(utc);
}

void ClockService::initialize_date_trust()
{
    date_trust_initialized_=true;
    date_configured_=false;
    marker_known_false_=false;
    basis_=basis_store_.load();
    if (!board_.rtc_available()) return;
    if (board_.rtc_voltage_low()) {
        marker_known_false_=date_store_.save_configured(false);
        return;
    }
    bool configured=false;
    if (!date_store_.load_configured(configured)) return;
    date_configured_=configured;
    marker_known_false_=!configured;
    if (basis_!=storage::RtcTimeBasis::LegacyLocal || !configured) return;

    // Existing valid RTC stores Polish local time. Only an unambiguous
    // timestamp may be migrated automatically; no guessing of autumn 02:xx.
    warsaw::DateTime local{};
    if (raw_rtc(local)) (void)write_local(local);
}

Reading ClockService::read() const
{
    Reading reading{};
    if (basis_==storage::RtcTimeBasis::LegacyLocal) {
        if (!board_.rtc_available()) return reading;
        if (board_.rtc_voltage_low()) {
            (void)invalidate_date_trust(false);
            return reading;
        }
        board::RtcTime raw{};
        if (!board_.read_rtc_time(raw)) return reading;
        reading.valid=true;
        reading.time={raw.hour,raw.minute};
        return reading;
    }
    warsaw::DateTime local{};
    if (!local_now(local)) return reading;
    reading.valid=true;
    reading.time={local.hour,local.minute};
    return reading;
}

bool ClockService::set_time(std::uint8_t hour,std::uint8_t minute) const
{
    last_local_status_=warsaw::LocalStatus::Invalid;
    if (hour>23 || minute>59 || !board_.rtc_available()) return false;
    warsaw::DateTime local{};
    if (local_now(local) && (basis_!=storage::RtcTimeBasis::LegacyLocal
                            || date_configured_)) {
        local.hour=hour; local.minute=minute; local.second=0;
        return write_local(local);
    }
    staged_local_.hour=hour;
    staged_local_.minute=minute;
    staged_local_.second=0;
    staged_time_=true;
    if (staged_date_) return write_local(staged_local_);
    return true; // Time retained in RAM until user also configures the date.
}

DateReading ClockService::read_date() const
{
    DateReading result{};
    warsaw::DateTime local{};
    if (!date_trust_initialized_ || !date_configured_
        || !local_now(local)) return result;
    result.valid=true;
    result.date={local.year,local.month,local.day};
    return result;
}

DateSetResult ClockService::set_date(const Date& date) const
{
    last_local_status_=warsaw::LocalStatus::Invalid;
    if (!date_trust_initialized_ || !valid_date(date)
        || !board_.rtc_available()) return DateSetResult::Failed;
    warsaw::DateTime local{};
    if (local_now(local) && date_configured_) {
        local.year=date.year; local.month=date.month; local.day=date.day;
    } else {
        staged_local_.year=date.year;
        staged_local_.month=date.month;
        staged_local_.day=date.day;
        staged_date_=true;
        if (staged_time_) local=staged_local_;
        else if (basis_==storage::RtcTimeBasis::LegacyLocal
            && !board_.rtc_voltage_low()) {
            board::RtcTime raw{};
            if (!board_.read_rtc_time(raw)) return DateSetResult::TimeUntrusted;
            local={date.year,date.month,date.day,raw.hour,raw.minute,raw.second};
        } else return DateSetResult::TimeUntrusted;
    }
    if (write_local(local)) return DateSetResult::Success;
    if (last_local_status_==warsaw::LocalStatus::Nonexistent)
        return DateSetResult::NonexistentLocalTime;
    if (last_local_status_==warsaw::LocalStatus::Ambiguous)
        return DateSetResult::AmbiguousLocalTime;
    return DateSetResult::Failed;
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
