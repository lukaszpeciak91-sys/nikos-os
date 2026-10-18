#pragma once

#include <cstddef>
#include <cstdint>

#include "board/board.hpp"
#include "clock/warsaw_time_rules.hpp"
#include "storage/rtc_time_basis_store.hpp"
#include "storage/rtc_date_configured_store.hpp"

namespace nikos::clock {

struct Time {
    std::uint8_t hour = 0;
    std::uint8_t minute = 0;
};

struct Reading {
    bool valid = false;
    Time time{};
};

struct Date {
    std::uint16_t year = 2000;
    std::uint8_t month = 1;
    std::uint8_t day = 1;
};

struct DateReading {
    bool valid = false;
    Date date{};
};

enum class DateSetResult : std::uint8_t {
    Success,
    TimeUntrusted,
    Failed,
    NonexistentLocalTime,
    AmbiguousLocalTime,
};

class ClockService final {
public:
    ClockService(
        board::Board& board,
        storage::RtcDateConfiguredStore& date_store);

    void initialize_date_trust();

    Reading read() const;
    bool set_time(std::uint8_t hour, std::uint8_t minute) const;

    DateReading read_date() const;
    bool read_utc(warsaw::DateTime& utc) const;
    bool pending_local_date() const { return staged_time_ && (basis_ != storage::RtcTimeBasis::Utc || !date_configured_); }
    warsaw::LocalStatus last_local_status() const { return last_local_status_; }
    DateSetResult set_date(const Date& date) const;

    static bool is_leap_year(std::uint16_t year);
    static std::uint8_t days_in_month(
        std::uint16_t year,
        std::uint8_t month);
    static bool valid_date(const Date& date);
    static std::uint8_t weekday_monday_index(const Date& date);

    static void format_hhmm(
        const Reading& reading,
        char* output,
        std::size_t output_size);
    static void format_ddmmyyyy(
        const DateReading& reading,
        char* output,
        std::size_t output_size);

private:
    bool raw_rtc(warsaw::DateTime& time) const;
    bool trusted_utc(warsaw::DateTime& time) const;
    bool local_now(warsaw::DateTime& time) const;
    bool commit_utc(const warsaw::DateTime& utc) const;
    bool write_local(const warsaw::DateTime& local) const;
    bool invalidate_date_trust(bool force_persist) const;

    board::Board& board_;
    storage::RtcDateConfiguredStore& date_store_;
    mutable bool date_trust_initialized_ = false;
    mutable bool date_configured_ = false;
    mutable bool marker_known_false_ = false;
    storage::RtcTimeBasisStore basis_store_;
    mutable storage::RtcTimeBasis basis_ = storage::RtcTimeBasis::Uncertain;
    mutable bool staged_time_ = false;
    mutable bool staged_date_ = false;
    mutable warsaw::DateTime staged_local_{};
    mutable warsaw::LocalStatus last_local_status_ = warsaw::LocalStatus::Invalid;
};

}  // namespace nikos::clock
