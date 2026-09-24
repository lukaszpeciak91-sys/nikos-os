#pragma once

#include <cstdint>

#include "ui_theme/ui_theme.hpp"

namespace nikos::settings {

enum class Orientation : std::uint8_t {
    Right,
    Left,
};

enum class SignalSound : std::uint8_t {
    Gentle,
    Classic,
    Pager,
};

enum class Brightness : std::uint8_t {
    Low,
    Medium,
    High,
};

struct BrightnessProfile {
    std::uint8_t active;
    std::uint8_t dimmed;
};

BrightnessProfile brightness_profile(Brightness brightness);

struct State {
    Orientation orientation = Orientation::Right;
    SignalSound signal_sound = SignalSound::Gentle;
    Brightness brightness = Brightness::Medium;
    ui_theme::Theme theme = ui_theme::Theme::Nikos;
};

}  // namespace nikos::settings
