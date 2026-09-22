#pragma once

#include <cstdint>

#include "board/board.hpp"

namespace nikos::power {

enum class DisplayState : std::uint8_t {
    Active,
    Dimmed,
    DisplayOff,
};

class DisplayLifecycle final {
public:
    explicit DisplayLifecycle(board::Board& board);

    void begin(std::uint32_t now_ms);
    void update(std::uint32_t now_ms);

    board::InputState filter_input(
        const board::InputState& input,
        std::uint32_t now_ms);

    void note_visible_activity(std::uint32_t now_ms);

    DisplayState state() const;

private:
    void enter_active(std::uint32_t now_ms);
    void enter_dimmed();
    void enter_display_off();

    board::Board& board_;
    DisplayState state_ = DisplayState::Active;
    std::uint32_t last_activity_ms_ = 0;
    bool suppress_wake_gesture_until_release_ = false;
};

}  // namespace nikos::power
