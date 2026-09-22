#pragma once

#include <cstdint>

#include "board/board.hpp"

namespace nikos::power {

enum class DisplayState : std::uint8_t {
    Active,
    Dimmed,
    DisplayOff,
};

enum class WakeReason : std::uint8_t {
    None,
    UserButton,
};

struct FilteredInput {
    board::InputState input{};
    WakeReason wake_reason = WakeReason::None;
    bool power_display_off = false;
};

class DisplayLifecycle final {
public:
    explicit DisplayLifecycle(board::Board& board);

    void begin();
    void update();

    FilteredInput filter_input(const board::InputState& input);

    void note_visible_activity();
    void display_off_now();
    void suppress_user_gesture_until_release();

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
