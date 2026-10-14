#include "settings/settings.hpp"

namespace nikos::settings {

BrightnessProfile brightness_profile(Brightness brightness)
{
    switch (brightness) {
        case Brightness::Low:
            return BrightnessProfile{72, 18};
        case Brightness::High:
            return BrightnessProfile{128, 32};
        case Brightness::Medium:
        default:
            return BrightnessProfile{96, 24};
    }
}

BrightnessProfile effective_brightness_profile(
    Brightness brightness,
    bool low_battery_reserve_active)
{
    return brightness_profile(
        low_battery_reserve_active ? Brightness::Low : brightness);
}

}  // namespace nikos::settings
