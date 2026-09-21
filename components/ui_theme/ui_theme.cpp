#include "ui_theme/ui_theme.hpp"

namespace {

// Experimental physical-LCD foundation: keep the base dark and let Accent
// distinguish themes. Product-semantic colors remain board-owned and fixed.
constexpr std::uint16_t kBackground = 0x0000;    // near #000000
constexpr std::uint16_t kSurface = 0x1082;       // near #101216
constexpr std::uint16_t kPrimaryText = 0xF77D;   // near #F0EEE8
constexpr std::uint16_t kSecondaryText = 0x8C93; // near #8C9199

constexpr nikos::ui_theme::Palette kNikosPalette{
    kBackground,
    kSurface,
    kPrimaryText,
    kSecondaryText,
    0x4DBB,  // cool blue/cyan experiment
};

constexpr nikos::ui_theme::Palette kAmberPalette{
    kBackground,
    kSurface,
    kPrimaryText,
    kSecondaryText,
    0xD4C8,  // amber experiment
};

constexpr nikos::ui_theme::Palette kGraphitePalette{
    kBackground,
    kSurface,
    kPrimaryText,
    kSecondaryText,
    0xA577,  // cool neutral/silver experiment
};

}  // namespace

namespace nikos::ui_theme {

const Palette& palette(Theme theme)
{
    switch (theme) {
        case Theme::Amber:
            return kAmberPalette;
        case Theme::Graphite:
            return kGraphitePalette;
        case Theme::Nikos:
        default:
            return kNikosPalette;
    }
}

}  // namespace nikos::ui_theme
