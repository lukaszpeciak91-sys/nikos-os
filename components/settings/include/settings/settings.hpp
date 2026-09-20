#pragma once

#include <cstdint>

#include "ui_theme/ui_theme.hpp"

namespace nikos::settings {

enum class SignalSound : std::uint8_t {
    Gentle,
    Classic,
    Pager,
};

struct State {
    SignalSound signal_sound = SignalSound::Gentle;
    ui_theme::Theme theme = ui_theme::Theme::Nikos;
};

}  // namespace nikos::settings
