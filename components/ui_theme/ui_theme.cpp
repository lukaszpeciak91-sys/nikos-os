#include "ui_theme/ui_theme.hpp"

namespace {

// Hardware-tuned RGB565 palettes for the M5StickC Plus SE ST7789V2 LCD.
// Surfaces stay intentionally close to black so theme identity comes from
// typography/accent and a restrained selection marker instead of bright fills.
constexpr nikos::ui_theme::Palette kNikosPalette{
    0x0000,  // Background    ~#000000
    0x10A2,  // Surface       ~#101410
    0xF77C,  // PrimaryText   ~#F6EEE6
    0x8C91,  // SecondaryText ~#8B918B
    0x7DD4,  // Accent        ~#7BBAA4
};

constexpr nikos::ui_theme::Palette kBursztynPalette{
    0x0000,  // Background    ~#000000
    0x1880,  // Surface       ~#181000
    0xEDA9,  // PrimaryText   ~#EEB64A
    0x8B25,  // SecondaryText ~#8B6529
    0xFDE9,  // Accent        ~#FFBE4A
};

constexpr nikos::ui_theme::Palette kMatrixPalette{
    0x0000,  // Background    ~#000000
    0x0081,  // Surface       ~#001008
    0x9F31,  // PrimaryText   ~#9CE68B
    0x43C9,  // SecondaryText ~#41794A
    0x674E,  // Accent        ~#62EA73
};

constexpr nikos::ui_theme::Palette kLavaPalette{
    0x0000,  // Background    ~#000000
    0x1840,  // Surface       ~#180800
    0xF6FA,  // PrimaryText   ~#F6DED5
    0x932B,  // SecondaryText ~#94655A
    0xF326,  // Accent        ~#F66531
};

constexpr nikos::ui_theme::Palette kNoirPalette{
    0x0000,  // Background    ~#000000
    0x1082,  // Surface       ~#101010
    0xF79E,  // PrimaryText   ~#F6F2F6
    0x7BEF,  // SecondaryText ~#7B7D7B
    0xFFFF,  // Accent        #FFFFFF
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
