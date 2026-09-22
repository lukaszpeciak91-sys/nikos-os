#include "ui_theme/ui_theme.hpp"

namespace {

// Experimental physical-LCD candidates. Every normal role changes between
// identities; product-semantic status colors remain board-owned and fixed.
constexpr nikos::ui_theme::Palette kNikosPalette{
    0x0001, 0x1082, 0xF77D, 0x8C93, 0x4DBB,
};

constexpr nikos::ui_theme::Palette kBursztynPalette{
    0x0820, 0x20C2, 0xFFBB, 0xBD2F, 0xFD80,
};

constexpr nikos::ui_theme::Palette kGraphitePalette{
    0x1082, 0x2125, 0xF7BF, 0x9D15, 0xC639,
};

constexpr nikos::ui_theme::Palette kLavaPalette{
    0x1021, 0x2862, 0xFF9B, 0xC4D1, 0xFA64,
};

constexpr nikos::ui_theme::Palette kMatrixPalette{
    0x0000, 0x00E1, 0xCFF9, 0x6D4E, 0x3FEB,
};

}  // namespace

namespace nikos::ui_theme {

const Palette& palette(Theme theme)
{
    switch (theme) {
        case Theme::Bursztyn:
            return kBursztynPalette;
        case Theme::Graphite:
            return kGraphitePalette;
        case Theme::Lava:
            return kLavaPalette;
        case Theme::Matrix:
            return kMatrixPalette;
        case Theme::Nikos:
        default:
            return kNikosPalette;
    }
}

}  // namespace nikos::ui_theme
