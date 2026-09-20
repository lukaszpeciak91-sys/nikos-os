#pragma once

#include <cstdint>

namespace nikos::ui_theme {

enum class Theme : std::uint8_t {
    Nikos,
    Amber,
    Graphite,
};

struct Palette {
    std::uint16_t background = 0;
    std::uint16_t surface = 0;
    std::uint16_t primary_text = 0;
    std::uint16_t secondary_text = 0;
    std::uint16_t accent = 0;
};

const Palette& palette(Theme theme);

}  // namespace nikos::ui_theme
