#pragma once
#include <cstdint>
#include <string>
namespace nikos::board {
struct InputState {
    bool primary_pressed = false;
    bool secondary_pressed = false;
    bool power_short = false;
    bool primary_short = false;
    bool primary_long = false;
    bool secondary_short = false;
    bool secondary_long = false;
};
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
    int marker_y = -1;
    int clear_calls = 0;
    std::string footer_text;
    std::string list_position;
    std::string reader_header;
    std::string reader_counter;
    std::string reader_page;
    std::string reader_side;
    std::string reader_m5;
    std::string quiz_header;
    void clear_screen() {
        ++clear_calls;
        marker_y = -1;
        footer_text.clear();
        list_position.clear();
        reader_header.clear();
        reader_counter.clear();
        reader_page.clear();
        reader_side.clear();
        reader_m5.clear();
        quiz_header.clear();
    }
    void draw_line(std::int16_t x0, std::int16_t y0,
        std::int16_t x1, std::int16_t y1, DisplayColor) {
        bounds_ok = bounds_ok && x0 >= 0 && x1 < 240
            && y0 >= 0 && y1 < 135;
        if (x0 == 8 && x1 == 8 && y0 >= 28) marker_y = y0;
    }
    void fill_rect(std::int16_t x, std::int16_t y,
                   std::int16_t w, std::int16_t h, DisplayColor) {
        bounds_ok = bounds_ok && x >= 0 && y >= 0
            && w > 0 && h > 0 && x + w <= 240 && y + h <= 135;
        pixels += w * h;
        if (x == 8 && w == 2 && h == 20) marker_y = y;
        if (w == 1 && h == 1) {
            if (x > glyph_pixel_max_x) glyph_pixel_max_x = x;
            if (x < glyph_pixel_min_x) glyph_pixel_min_x = x;
        }
    }
    void draw_text_region(std::int16_t x, std::int16_t y,
                          std::int16_t w, std::int16_t h,
                          const char* text, std::uint8_t,
                          DisplayColor = DisplayColor::PrimaryText,
                          DisplayColor = DisplayColor::Background) {
        bounds_ok = bounds_ok && x >= 0 && y >= 0
            && w > 0 && h > 0 && x + w <= 240 && y + h <= 135;
        ++text_calls;
        if (x == 14 && y == 121) footer_text = text;
        if (x == 176 && y == 4) list_position = text;
        if (x == 181 && y == 2) reader_counter = text;
        if (x == 8 && y == 2) quiz_header = text;
        if (x == 5 && y == 123) reader_page = text;
        if (x == 49 && y == 123) reader_side = text;
        if (x == 133 && y == 123) reader_m5 = text;
    }
    void draw_polish_ui_text_region(std::int16_t x, std::int16_t y,
                                   std::int16_t w, std::int16_t h,
                                   const char* text, std::uint8_t size,
                                   DisplayColor fg = DisplayColor::PrimaryText,
                                   DisplayColor bg = DisplayColor::Background) {
        draw_text_region(x, y, w, h, text, size, fg, bg);
        if (x == 4 && y == 2) reader_header = text;
        if (y >= 120) footer_text = text;
    }
};
}
