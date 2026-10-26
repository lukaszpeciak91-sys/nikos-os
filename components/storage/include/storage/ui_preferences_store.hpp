#pragma once

#include "settings/settings.hpp"

namespace nikos::storage {

// Four independent, bounded, per-device NVS preferences.
// The existing settings::State remains the single runtime source of truth.
class UiPreferencesStore final {
public:
    ui_theme::Theme load_theme() const;
    settings::Orientation load_orientation() const;
    settings::Brightness load_brightness() const;
    settings::SignalSound load_signal_sound() const;

    bool save_theme(ui_theme::Theme value) const;
    bool save_orientation(settings::Orientation value) const;
    bool save_brightness(settings::Brightness value) const;
    bool save_signal_sound(settings::SignalSound value) const;
};

}  // namespace nikos::storage
