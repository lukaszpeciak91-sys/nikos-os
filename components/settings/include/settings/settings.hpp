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

struct State {
    Orientation orientation = Orientation::Right;
    SignalSound signal_sound = SignalSound::Gentle;
    ui_theme::Theme theme = ui_theme::Theme::Nikos;
};

}  // namespace nikos::settings
