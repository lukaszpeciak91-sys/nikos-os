#include "clock/clock_service.hpp"

#include <cstdio>

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

    const Reading verified = read();
    return verified.valid
        && verified.time.hour == hour
        && verified.time.minute == minute;
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

}  // namespace nikos::clock
