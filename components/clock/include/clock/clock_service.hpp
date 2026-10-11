#pragma once

#include <cstddef>
#include <cstdint>

#include "board/board.hpp"

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

class ClockService final {
public:
    explicit ClockService(board::Board& board);

    Reading read() const;
    bool set_time(std::uint8_t hour, std::uint8_t minute) const;

    DateReading read_date() const;
    bool set_date(const Date& date) const;

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
    board::Board& board_;
};

}  // namespace nikos::clock
