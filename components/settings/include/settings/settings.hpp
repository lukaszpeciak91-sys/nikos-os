#pragma once

#include <cstdint>

#include "ui_theme/ui_theme.hpp"
#include "font_core/font_core.hpp"

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

enum class CommunicatorView : std::uint8_t {
    List,
    Single,
};

struct BrightnessProfile {
    std::uint8_t active;
    std::uint8_t dimmed;
};

BrightnessProfile brightness_profile(Brightness brightness);
BrightnessProfile effective_brightness_profile(
    Brightness brightness,
    bool low_battery_reserve_active);

struct State {
    Orientation orientation = Orientation::Right;
    SignalSound signal_sound = SignalSound::Gentle;
    std::uint8_t signal_volume_level = 4; // 100% preset is legacy M5Unified volume 64.
    Brightness brightness = Brightness::Medium;
    CommunicatorView communicator_view = CommunicatorView::List;
    ui_theme::Theme theme = ui_theme::Theme::Nikos;
    bool auto_power_off_enabled = true;
    font_core::Mode font_mode = font_core::Mode::Standard;
};

}  // namespace nikos::settings
