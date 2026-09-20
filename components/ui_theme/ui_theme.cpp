#include "ui_theme/ui_theme.hpp"

namespace {

constexpr nikos::ui_theme::Palette kNikosPalette{
    0x10A5,  // #11162F
    0x1908,  // #1A2144
    0xF75B,  // #F1EBDD
    0x6391,  // #65708B
    0x8DF5,  // #88BDA8
};

constexpr nikos::ui_theme::Palette kAmberPalette{
    0x1081,  // #17120D
    0x2902,  // #2A2117
    0xF739,  // #F3E7CC
    0x8BAC,  // #8B7760
    0xC4A9,  // #C7944B
};

constexpr nikos::ui_theme::Palette kGraphitePalette{
    0x10A2,  // #151617
    0x2145,  // #25282B
    0xEF7C,  // #EEECE6
    0x7C10,  // #7A8087
    0x8494,  // #8393A3
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
