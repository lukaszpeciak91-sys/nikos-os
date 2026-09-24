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

}  // namespace nikos::settings
