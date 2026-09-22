#pragma once

#include <cstdint>

#include "ui_theme/ui_theme.hpp"

namespace nikos::board {

enum class ChargeState : std::uint8_t {
    Unknown,
    Charging,
    Discharging,
};

enum class DisplayOrientation : std::uint8_t {
    Right,
    Left,
};

enum class DisplayColor : std::uint8_t {
    Background,
    Surface,
    PrimaryText,
    SecondaryText,
    Accent,
    StatusActive,
    StatusInactive,
    Attention,
    Danger,
};

struct PowerStatus {
    std::int16_t voltage_mv = -1;
    std::int32_t level_percent = -1;
    ChargeState charge_state = ChargeState::Unknown;
};

struct RtcTime {
    std::uint8_t hour = 0;
    std::uint8_t minute = 0;
    std::uint8_t second = 0;
};

struct InputState {
    bool primary_pressed = false;
    bool secondary_pressed = false;
    bool primary_short = false;
    bool primary_long = false;
    bool secondary_short = false;
    bool secondary_long = false;
};

class Board final {
public:
    bool begin();
    InputState poll_input();

    bool initialize_rtc();
    bool rtc_available() const;
    bool rtc_voltage_low() const;
    bool read_rtc_time(RtcTime& time) const;
    bool write_rtc_time(const RtcTime& time);

    PowerStatus power_status() const;
    void tone(float frequency_hz, std::uint32_t duration_ms);
    void stop_tone();
    void wake_display();
    void dim_display();
    void sleep_display();
    void power_off();

    void set_theme(ui_theme::Theme theme);
    void set_display_orientation(DisplayOrientation orientation);

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
        DisplayColor foreground = DisplayColor::PrimaryText,
        DisplayColor background = DisplayColor::Background);
    void draw_polish_ui_text_region(
        std::int16_t x,
        std::int16_t y,
        std::int16_t width,
        std::int16_t height,
        const char* utf8_text,
        std::uint8_t text_scale = 1,
        DisplayColor foreground = DisplayColor::PrimaryText,
        DisplayColor background = DisplayColor::Background);
    void draw_polish_ui_font_sanity_demo();

    void draw_screen(const char* title, const char* body);
    const char* detected_board_name() const;

private:
    std::uint32_t resolve_display_color(DisplayColor color) const;

    ui_theme::Theme theme_ = ui_theme::Theme::Nikos;
    bool rtc_available_ = false;
};

const char* charge_state_name(ChargeState state);

}  // namespace nikos::board
