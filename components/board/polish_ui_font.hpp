#pragma once

#include <cstddef>

#include "M5GFX.h"

namespace nikos::board::detail {

extern const lgfx::GFXfont kPolishUiFont;

std::size_t polish_ui_font_resource_bytes();

}  // namespace nikos::board::detail
