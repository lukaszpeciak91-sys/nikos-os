#include "board/board.hpp"

#include "polish_ui_font.hpp"

#include "M5Unified.h"
#include "esp_log.h"

namespace {

constexpr char kTag[] = "board";
constexpr std::uint32_t kHoldThresholdMs = 600;
constexpr std::uint8_t kDisplayActiveBrightness = 128;
constexpr std::uint8_t kDisplayDimBrightness = 32;

// Theme-varying colors come from ui_theme::Palette. Product-semantic colors
// remain fixed across themes.
constexpr std::uint16_t kStatusActiveRgb565 = 0x8DF5;   // #88BDA8
constexpr std::uint16_t kStatusInactiveRgb565 = 0xB36D; // #B46F6F
constexpr std::uint16_t kAttentionRgb565 = 0xFD20;      // existing SYGNAŁ orange
constexpr std::uint16_t kDangerRgb565 = 0xC2EB;         // #C65F5F

constexpr char kPolishFontSanityText[] =
    "ĄĆĘŁŃÓŚŹŻ ąćęłńóśźż CZEŚĆ! MOŻESZ GADAĆ?";


}  // namespace

namespace nikos::board {

bool Board::begin()
{
    auto config = M5.config();
    config.internal_imu = false;
    config.internal_mic = false;
    config.internal_rtc = false;
    config.internal_spk = true;

    M5.begin(config);

    set_display_orientation(DisplayOrientation::Right);
    M5.Display.setBrightness(kDisplayActiveBrightness);
    M5.Display.setTextWrap(false);

    M5.BtnA.setHoldThresh(kHoldThresholdMs);
    M5.BtnB.setHoldThresh(kHoldThresholdMs);

    ESP_LOGI(
        kTag,
        "M5Unified board id: %d (%s)",
        static_cast<int>(M5.getBoard()),
        detected_board_name());

    tone(3200.0F, 90);
    return M5.Display.width() > 0 && M5.Display.height() > 0;
}

InputState Board::poll_input()
{
    M5.update();

    InputState state;

    // Physical mapping verified on M5StickC Plus SE:
    // - M5-marked user button -> primary
    // - opposite-side user button -> secondary
    // POWER is a separate system display control, not a third navigation
    // button. Expose only its short click; long-power behavior remains owned
    // by M5Unified / the board PMIC.
    state.primary_pressed = M5.BtnA.isPressed();
    state.secondary_pressed = M5.BtnB.isPressed();
    state.power_short = M5.BtnPWR.wasClicked();
    state.primary_long = M5.BtnA.wasHold();
    state.secondary_long = M5.BtnB.wasHold();
    state.primary_short =
        M5.BtnA.wasClicked() && !M5.BtnA.wasReleasedAfterHold();
    state.secondary_short =
        M5.BtnB.wasClicked() && !M5.BtnB.wasReleasedAfterHold();
    return state;
}

bool Board::initialize_rtc()
{
    rtc_available_ =
        M5.In_I2C.isEnabled()
        && M5.Rtc.begin(&M5.In_I2C, M5.getBoard());

    if (!rtc_available_) {
        ESP_LOGW(kTag, "RTC initialization failed; clock unavailable");
    }
    return rtc_available_;
}

bool Board::rtc_available() const
{
    return rtc_available_ && M5.Rtc.isEnabled();
}

bool Board::rtc_voltage_low() const
{
    return !rtc_available() || M5.Rtc.getVoltLow();
}

bool Board::read_rtc_time(RtcTime& time) const
{
    if (!rtc_available()) {
        return false;
    }

    m5::rtc_time_t rtc_time;
    if (!M5.Rtc.getTime(&rtc_time)) {
        return false;
    }

    if (rtc_time.hours < 0
        || rtc_time.hours > 23
        || rtc_time.minutes < 0
        || rtc_time.minutes > 59
        || rtc_time.seconds < 0
        || rtc_time.seconds > 59) {
        return false;
    }

    time.hour = static_cast<std::uint8_t>(rtc_time.hours);
    time.minute = static_cast<std::uint8_t>(rtc_time.minutes);
    time.second = static_cast<std::uint8_t>(rtc_time.seconds);
    return true;
}

bool Board::write_rtc_time(const RtcTime& time)
{
    if (!rtc_available()
        || time.hour > 23
        || time.minute > 59
        || time.second > 59) {
        return false;
    }

    const m5::rtc_time_t rtc_time(
        static_cast<std::int8_t>(time.hour),
        static_cast<std::int8_t>(time.minute),
        static_cast<std::int8_t>(time.second));
    M5.Rtc.setTime(rtc_time);
    return true;
}

PowerStatus Board::power_status() const
{
    PowerStatus status;

    const std::int16_t voltage_mv = M5.Power.getBatteryVoltage();
    if (voltage_mv <= 0) {
        // AXP192 register-read failures may surface as 0 mV. Keep the whole
        // battery sample explicitly invalid so downstream policy can never
        // mistake an I2C failure for a deeply discharged battery.
        return status;
    }

    status.voltage_mv = voltage_mv;
    status.level_percent = M5.Power.getBatteryLevel();

    switch (M5.Power.isCharging()) {
        case m5::Power_Class::is_charging:
            status.charge_state = ChargeState::Charging;
            break;
        case m5::Power_Class::is_discharging:
            status.charge_state = ChargeState::Discharging;
            break;
        case m5::Power_Class::charge_unknown:
        default:
            status.charge_state = ChargeState::Unknown;
            break;
    }

    return status;
}

void Board::tone(float frequency_hz, std::uint32_t duration_ms)
{
    if (!M5.Speaker.tone(frequency_hz, duration_ms)) {
        ESP_LOGW(kTag, "Buzzer tone request was not accepted");
    }
}

void Board::stop_tone()
{
    M5.Speaker.stop();
}

void Board::wake_display()
{
    // M5GFX wakeup() restores its remembered brightness. Always override it
    // with the product ACTIVE level so a prior dim state cannot survive wake.
    M5.Display.wakeup();
    M5.Display.setBrightness(kDisplayActiveBrightness);
}

void Board::dim_display()
{
    M5.Display.setBrightness(kDisplayDimBrightness);
}

void Board::sleep_display()
{
    M5.Display.sleep();
}

void Board::power_off()
{
    M5.Power.powerOff();
}

void Board::set_theme(ui_theme::Theme theme)
{
    theme_ = theme;
}

void Board::set_display_orientation(DisplayOrientation orientation)
{
    M5.Display.setRotation(
        orientation == DisplayOrientation::Left ? 3 : 1);
}

void Board::clear_screen()
{
    M5.Display.fillScreen(resolve_display_color(DisplayColor::Background));
}

void Board::fill_circle(
    std::int16_t x,
    std::int16_t y,
    std::int16_t radius,
    DisplayColor color)
{
    M5.Display.fillCircle(x, y, radius, resolve_display_color(color));
}

void Board::draw_line(
    std::int16_t x0,
    std::int16_t y0,
    std::int16_t x1,
    std::int16_t y1,
    DisplayColor color)
{
    M5.Display.drawLine(x0, y0, x1, y1, resolve_display_color(color));
}

void Board::draw_text_region(
    std::int16_t x,
    std::int16_t y,
    std::int16_t width,
    std::int16_t height,
    const char* text,
    std::uint8_t text_size,
    DisplayColor foreground,
    DisplayColor background)
{
    auto& display = M5.Display;
    const std::uint32_t foreground_color = resolve_display_color(foreground);
    const std::uint32_t background_color = resolve_display_color(background);

    display.fillRect(x, y, width, height, background_color);
    // Normal product UI uses the native/default M5GFX font deterministically.
    // The Polish-font experiment remains available only through its explicit
    // scoped helper below.
    display.setFont(&fonts::Font0);
    display.setTextColor(foreground_color, background_color);
    display.setTextSize(text_size);
    display.setCursor(x, y);
    display.print(text);
}

void Board::draw_polish_ui_text_region(
    std::int16_t x,
    std::int16_t y,
    std::int16_t width,
    std::int16_t height,
    const char* utf8_text,
    std::uint8_t text_scale,
    DisplayColor foreground,
    DisplayColor background)
{
    auto& display = M5.Display;

    const lgfx::IFont* previous_font = display.getFont();
    const lgfx::TextStyle previous_style = display.getTextStyle();
    const std::int32_t previous_cursor_x = display.getCursorX();
    const std::int32_t previous_cursor_y = display.getCursorY();

    const std::uint32_t foreground_color = resolve_display_color(foreground);
    const std::uint32_t background_color = resolve_display_color(background);

    display.fillRect(x, y, width, height, background_color);
    display.setFont(&detail::kPolishUiFont);
    display.setTextColor(foreground_color, background_color);
    display.setTextSize(text_scale == 0 ? 1 : text_scale);
    display.setCursor(x, y);
    display.print(utf8_text == nullptr ? "" : utf8_text);

    display.setFont(previous_font);
    display.setTextStyle(previous_style);
    display.setCursor(previous_cursor_x, previous_cursor_y);
}

void Board::draw_polish_ui_font_sanity_demo()
{
    M5.Display.fillScreen(resolve_display_color(DisplayColor::Background));
    draw_polish_ui_text_region(
        4,
        58,
        232,
        16,
        kPolishFontSanityText,
        1,
        DisplayColor::PrimaryText,
        DisplayColor::Background);
}

void Board::draw_screen(const char* title, const char* body)
{
    auto& display = M5.Display;
    const std::uint32_t background =
        resolve_display_color(DisplayColor::Background);
    const std::uint32_t foreground =
        resolve_display_color(DisplayColor::PrimaryText);

    display.fillScreen(background);
    display.setFont(&fonts::Font0);
    display.setTextColor(foreground, background);
    display.setCursor(4, 3);
    display.setTextSize(2);
    display.println(title);

    display.setCursor(4, 22);
    display.setTextSize(1);
    display.print(body);
}

std::uint32_t Board::resolve_display_color(DisplayColor color) const
{
    const ui_theme::Palette& palette = ui_theme::palette(theme_);

    switch (color) {
        case DisplayColor::Surface:
            return palette.surface;
        case DisplayColor::PrimaryText:
            return palette.primary_text;
        case DisplayColor::SecondaryText:
            return palette.secondary_text;
        case DisplayColor::Accent:
            return palette.accent;
        case DisplayColor::StatusActive:
            return kStatusActiveRgb565;
        case DisplayColor::StatusInactive:
            return kStatusInactiveRgb565;
        case DisplayColor::Attention:
            return kAttentionRgb565;
        case DisplayColor::Danger:
            return kDangerRgb565;
        case DisplayColor::Background:
        default:
            return palette.background;
    }
}

const char* Board::detected_board_name() const
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5StickC:
            return "M5StickC";
        case m5::board_t::board_M5StickCPlus:
            return "M5StickCPlus";
        case m5::board_t::board_M5StickCPlus2:
            return "M5StickCPlus2";
        default:
            return "unexpected";
    }
}

const char* charge_state_name(ChargeState state)
{
    switch (state) {
        case ChargeState::Charging:
            return "CHG";
        case ChargeState::Discharging:
            return "DIS";
        case ChargeState::Unknown:
        default:
            return "UNK";
    }
}

}  // namespace nikos::board
