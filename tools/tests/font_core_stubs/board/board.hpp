#pragma once
#include <cstdint>
#include <string>
namespace nikos::board {
enum class DisplayColor : std::uint8_t {
    Background, Surface, PrimaryText, SecondaryText, Accent, StatusActive,
    StatusInactive, Attention, Danger
};
class Board {
public:
    int pixels = 0;
    int text_calls = 0;
    int glyph_pixel_max_x = -1;
    int glyph_pixel_min_x = 240;
    bool bounds_ok = true;
    void fill_rect(std::int16_t x, std::int16_t y,
                   std::int16_t w, std::int16_t h, DisplayColor) {
        bounds_ok = bounds_ok && x >= 0 && y >= 0
            && w > 0 && h > 0 && x + w <= 240 && y + h <= 135;
        pixels += w * h;
        if (w == 1 && h == 1) {
            if (x > glyph_pixel_max_x) glyph_pixel_max_x = x;
            if (x < glyph_pixel_min_x) glyph_pixel_min_x = x;
        }
    }
    void draw_text_region(std::int16_t x, std::int16_t y,
                          std::int16_t w, std::int16_t h,
                          const char*, std::uint8_t, DisplayColor,
                          DisplayColor) {
        bounds_ok = bounds_ok && x >= 0 && y >= 0
            && w > 0 && h > 0 && x + w <= 240 && y + h <= 135;
        ++text_calls;
    }
};
}
