#pragma once

#include <cstdint>

namespace nikos::board {

enum class ChargeState : std::uint8_t {
    Unknown,
    Charging,
    Discharging,
};

enum class DisplayColor : std::uint8_t {
    Black,
    White,
    Red,
    Green,
    Navy,
    PanelNavy,
    Ivory,
    AccentGreen,
    MutedBlue,
};

struct PowerStatus {
    std::int16_t voltage_mv = -1;
    std::int32_t level_percent = -1;
    ChargeState charge_state = ChargeState::Unknown;
};

struct InputState {
    bool primary_short = false;
    bool primary_long = false;
    bool secondary_short = false;
    bool secondary_long = false;
};

class Board final {
public:
    bool begin();
    InputState poll_input();

    PowerStatus power_status() const;
    void tone(float frequency_hz, std::uint32_t duration_ms);

    void clear_screen();
    void fill_circle(
        std::int16_t x,
        std::int16_t y,
        std::int16_t radius,
        DisplayColor color);
    void draw_line(
        std::int16_t x0,
        std::int16_t y0,
        std::int16_t x1,
        std::int16_t y1,
        DisplayColor color);
    void draw_text_region(
        std::int16_t x,
        std::int16_t y,
        std::int16_t width,
        std::int16_t height,
        const char* text,
        std::uint8_t text_size,
        DisplayColor foreground = DisplayColor::White,
        DisplayColor background = DisplayColor::Black);
    void draw_polish_ui_text_region(
        std::int16_t x,
        std::int16_t y,
        std::int16_t width,
        std::int16_t height,
        const char* utf8_text,
        std::uint8_t text_scale = 1,
        DisplayColor foreground = DisplayColor::White,
        DisplayColor background = DisplayColor::Black);
    void draw_polish_ui_font_sanity_demo();

    void draw_screen(const char* title, const char* body);
    const char* detected_board_name() const;
};

const char* charge_state_name(ChargeState state);

}  // namespace nikos::board
