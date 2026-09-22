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

    void begin();
    void update();

    board::InputState filter_input(const board::InputState& input);

    void note_visible_activity();

    DisplayState state() const;

private:
    static std::uint32_t now_ms();

    void enter_active(std::uint32_t now_ms);
    void enter_dimmed();
    void enter_display_off();

    board::Board& board_;
    DisplayState state_ = DisplayState::Active;
    std::uint32_t last_activity_ms_ = 0;
    bool suppress_wake_gesture_until_release_ = false;
};

}  // namespace nikos::power
