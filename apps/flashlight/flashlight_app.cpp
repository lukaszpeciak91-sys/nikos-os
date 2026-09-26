#include "flashlight/flashlight_app.hpp"

namespace {

void draw_selection_marker(
    nikos::board::Board& board,
    std::int16_t x,
    std::int16_t y,
    std::int16_t height)
{
    const std::int16_t bottom =
        static_cast<std::int16_t>(y + height - 1);
    board.draw_line(
        x,
        y,
        x,
        bottom,
        nikos::board::DisplayColor::Accent);
    board.draw_line(
        static_cast<std::int16_t>(x + 1),
        y,
        static_cast<std::int16_t>(x + 1),
        bottom,
        nikos::board::DisplayColor::Accent);
}

}  // namespace

namespace nikos::flashlight {

FlashlightApp::FlashlightApp(
    board::Board& board,
    power::DisplayLifecycle& display_lifecycle)
    : board_(board),
      display_lifecycle_(display_lifecycle)
{
}

void FlashlightApp::begin()
{
    active_ = true;
    state_ = State::SelectBrightness;
    render_selector();
}

void FlashlightApp::end()
{
    prepare_for_foreground_takeover();
    active_ = false;
}

void FlashlightApp::redraw()
{
    if (!active_) {
        return;
    }

    if (state_ == State::LightOn) {
        board_.set_display_brightness_override(
            kBrightnessLevels[selection_]);
        board_.fill_flashlight_white();
        return;
    }

    render_selector();
}

FlashlightApp::UpdateResult FlashlightApp::update(
    const board::InputState& input)
{
    if (!active_) {
        return UpdateResult::ExitRequested;
    }

    if (state_ == State::LightOn) {
        if (input.secondary_long) {
            stop_light(false);
            return UpdateResult::ExitRequested;
        }

        if (input.primary_short
            || input.primary_long
            || input.secondary_short) {
            stop_light(true);
            return UpdateResult::Running;
        }

        // Keep normal inactivity policy alive through its existing owner.
        // POWER is still filtered globally before this update path.
        display_lifecycle_.note_visible_activity();
        return UpdateResult::Running;
    }

    if (input.secondary_long) {
        return UpdateResult::ExitRequested;
    }

    if (input.secondary_short) {
        selection_ =
            static_cast<std::uint8_t>((selection_ + 1U) % 4U);
        render_selector();
        return UpdateResult::Running;
    }

    if (input.primary_short) {
        if (selection_ < 3U) {
            start_light();
        } else {
            return UpdateResult::ExitRequested;
        }
    }

    return UpdateResult::Running;
}

void FlashlightApp::prepare_for_foreground_takeover()
{
    if (state_ != State::LightOn) {
        return;
    }

    board_.restore_display_brightness();
    state_ = State::SelectBrightness;
}

bool FlashlightApp::light_on() const
{
    return state_ == State::LightOn;
}

void FlashlightApp::start_light()
{
    display_lifecycle_.note_visible_activity();
    board_.set_display_brightness_override(
        kBrightnessLevels[selection_]);
    board_.fill_flashlight_white();
    state_ = State::LightOn;
}

void FlashlightApp::stop_light(bool render_selector)
{
    board_.restore_display_brightness();
    state_ = State::SelectBrightness;
    display_lifecycle_.note_visible_activity();

    if (render_selector) {
        render_selector();
    }
}

void FlashlightApp::render_selector()
{
    board_.clear_screen();

    board_.draw_text_region(
        14,
        10,
        212,
        20,
        "LATARKA",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        14,
        30,
        212,
        14,
        "JASNOSC",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    constexpr const char* kItems[4] = {
        "50%",
        "75%",
        "100%",
        "POWROT",
    };

    for (std::uint8_t index = 0; index < 4; ++index) {
        const bool selected = index == selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(45 + index * 18);

        board_.draw_text_region(
            28,
            y,
            184,
            17,
            kItems[index],
            2,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            draw_selection_marker(board_, 18, y, 16);
        }
    }

    board_.draw_text_region(
        14,
        120,
        212,
        12,
        "M5 WLACZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

}  // namespace nikos::flashlight
