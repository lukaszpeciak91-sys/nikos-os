#include "power/display_lifecycle.hpp"

#include "esp_timer.h"

namespace {

constexpr std::uint32_t kDimAfterMs = 15000;
constexpr std::uint32_t kDisplayOffAfterMs = 45000;

bool has_user_button_event(const nikos::board::InputState& input)
{
    return input.primary_short
        || input.primary_long
        || input.secondary_short
        || input.secondary_long;
}

bool any_user_button_pressed(const nikos::board::InputState& input)
{
    return input.primary_pressed || input.secondary_pressed;
}

}  // namespace

namespace nikos::power {

DisplayLifecycle::DisplayLifecycle(board::Board& board)
    : board_(board)
{
}

void DisplayLifecycle::begin()
{
    suppress_wake_gesture_until_release_ = false;
    enter_active(now_ms());
}

void DisplayLifecycle::update()
{
    const std::uint32_t now = now_ms();
    if (state_ == DisplayState::DisplayOff) {
        return;
    }

    const std::uint32_t inactive_ms = now - last_activity_ms_;
    if (inactive_ms >= kDisplayOffAfterMs) {
        enter_display_off();
        return;
    }

    if (state_ == DisplayState::Active
        && inactive_ms >= kDimAfterMs) {
        enter_dimmed();
    }
}

FilteredInput DisplayLifecycle::filter_input(
    const board::InputState& input)
{
    const std::uint32_t now = now_ms();
    const bool button_pressed = any_user_button_pressed(input);
    const bool button_event = has_user_button_event(input);

    if (suppress_wake_gesture_until_release_) {
        if (!button_pressed) {
            suppress_wake_gesture_until_release_ = false;
        }
        return FilteredInput{};
    }

    if (state_ == DisplayState::DisplayOff) {
        if (button_pressed || button_event) {
            enter_active(now);

            // The wake press owns the entire physical gesture. Keep all
            // click/hold/release-derived events suppressed until both user
            // buttons are physically released.
            suppress_wake_gesture_until_release_ = button_pressed;
            return FilteredInput{
                board::InputState{},
                WakeReason::UserButton,
            };
        }
        return FilteredInput{};
    }

    if (button_pressed || button_event) {
        note_visible_activity();
    }

    return FilteredInput{
        input,
        WakeReason::None,
    };
}

void DisplayLifecycle::note_visible_activity()
{
    const std::uint32_t now = now_ms();
    if (state_ != DisplayState::Active) {
        enter_active(now);
        return;
    }

    last_activity_ms_ = now;
}

void DisplayLifecycle::display_off_now()
{
    enter_display_off();
}

void DisplayLifecycle::suppress_user_gesture_until_release()
{
    suppress_wake_gesture_until_release_ = true;
}


DisplayState DisplayLifecycle::state() const
{
    return state_;
}

std::uint32_t DisplayLifecycle::now_ms()
{
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000U);
}

void DisplayLifecycle::enter_active(std::uint32_t now_ms)
{
    board_.wake_display();
    state_ = DisplayState::Active;
    last_activity_ms_ = now_ms;
}

void DisplayLifecycle::enter_dimmed()
{
    board_.dim_display();
    state_ = DisplayState::Dimmed;
}

void DisplayLifecycle::enter_display_off()
{
    board_.sleep_display();
    state_ = DisplayState::DisplayOff;
}

}  // namespace nikos::power
