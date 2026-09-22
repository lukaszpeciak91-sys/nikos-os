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

class ClockService final {
public:
    explicit ClockService(board::Board& board);

    Reading read() const;
    bool set_time(std::uint8_t hour, std::uint8_t minute) const;

    static void format_hhmm(
        const Reading& reading,
        char* output,
        std::size_t output_size);

private:
    board::Board& board_;
};

}  // namespace nikos::clock
