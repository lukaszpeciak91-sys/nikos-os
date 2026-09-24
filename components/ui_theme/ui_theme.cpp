#include "ui_theme/ui_theme.hpp"

namespace {

constexpr std::uint16_t rgb565(
    std::uint8_t red,
    std::uint8_t green,
    std::uint8_t blue)
{
    return static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(red >> 3U) << 11U)
        | (static_cast<std::uint16_t>(green >> 2U) << 5U)
        | static_cast<std::uint16_t>(blue >> 3U));
}

// Hardware-tuned RGB565 palettes for the M5StickC Plus SE ST7789V2 LCD.
// Background and Surface intentionally carry the theme identity so the whole
// screen remains recognizable even without foreground content.
constexpr nikos::ui_theme::Palette kNikosPalette{
    rgb565(4, 8, 16),       // Background    #040810 -> 0x0042
    rgb565(16, 24, 40),     // Surface       #101828 -> 0x10C5
    rgb565(240, 238, 232),  // PrimaryText   #F0EEE8 -> 0xF77D
    rgb565(140, 145, 153),  // SecondaryText #8C9199 -> 0x8C93
    rgb565(112, 184, 152),  // Accent        #70B898 -> 0x75D3
};

constexpr nikos::ui_theme::Palette kBursztynPalette{
    rgb565(16, 8, 0),       // Background    #100800 -> 0x1040
    rgb565(32, 20, 8),      // Surface       #201408 -> 0x20A1
    rgb565(240, 208, 128),  // PrimaryText   #F0D080 -> 0xF690
    rgb565(152, 112, 56),   // SecondaryText #987038 -> 0x9B87
    rgb565(248, 184, 64),   // Accent        #F8B840 -> 0xFDC8
};

constexpr nikos::ui_theme::Palette kMatrixPalette{
    rgb565(0, 16, 8),       // Background    #001008 -> 0x0081
    rgb565(8, 32, 16),      // Surface       #082010 -> 0x0902
    rgb565(168, 232, 160),  // PrimaryText   #A8E8A0 -> 0xAF54
    rgb565(72, 128, 80),    // SecondaryText #488050 -> 0x4C0A
    rgb565(96, 224, 112),   // Accent        #60E070 -> 0x670E
};

constexpr nikos::ui_theme::Palette kLavaPalette{
    rgb565(24, 4, 4),       // Background    #180404 -> 0x1820
    rgb565(48, 12, 8),      // Surface       #300C08 -> 0x3061
    rgb565(240, 224, 216),  // PrimaryText   #F0E0D8 -> 0xF71B
    rgb565(152, 96, 88),    // SecondaryText #986058 -> 0x9B0B
    rgb565(240, 88, 40),    // Accent        #F05828 -> 0xF2C5
};

constexpr nikos::ui_theme::Palette kNoirPalette{
    rgb565(0, 0, 0),        // Background    #000000 -> 0x0000
    rgb565(24, 24, 24),     // Surface       #181818 -> 0x18C3
    rgb565(247, 247, 247),  // PrimaryText   #F7F7F7 -> 0xF7BE
    rgb565(132, 132, 132),  // SecondaryText #848484 -> 0x8430
    rgb565(231, 231, 231),  // Accent        #E7E7E7 -> 0xE73C
};

}  // namespace

namespace nikos::ui_theme {

const Palette& palette(Theme theme)
{
    switch (theme) {
        case Theme::Bursztyn:
            return kBursztynPalette;
        case Theme::Matrix:
            return kMatrixPalette;
        case Theme::Lava:
            return kLavaPalette;
        case Theme::Noir:
            return kNoirPalette;
        case Theme::Nikos:
        default:
            return kNikosPalette;
    }
}

}  // namespace nikos::ui_theme
