#include "board/board.hpp"

#include "polish_ui_font.hpp"

#include "M5Unified.h"
#include "esp_log.h"

namespace {

constexpr char kTag[] = "board";
constexpr std::uint32_t kHoldThresholdMs = 600;
constexpr char kPolishFontSanityText[] =
    u8"ĄĆĘŁŃÓŚŹŻ ąćęłńóśźż CZEŚĆ! MOŻESZ GADAĆ?";

std::uint32_t to_display_color(nikos::board::DisplayColor color)
{
    switch (color) {
        case nikos::board::DisplayColor::White:
            return TFT_WHITE;
        case nikos::board::DisplayColor::Red:
            return TFT_RED;
        case nikos::board::DisplayColor::Green:
            return TFT_GREEN;
        case nikos::board::DisplayColor::Navy:
            return 0x08C4;
        case nikos::board::DisplayColor::PanelNavy:
            return 0x0927;
        case nikos::board::DisplayColor::Ivory:
            return 0xF75B;
        case nikos::board::DisplayColor::AccentGreen:
            return 0x9E91;
        case nikos::board::DisplayColor::MutedBlue:
            return 0x7C95;
        case nikos::board::DisplayColor::Black:
        default:
            return TFT_BLACK;
    }
}

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

    M5.Display.setRotation(1);
    M5.Display.setBrightness(128);
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
    // The separate power button is intentionally not exposed through InputState.
    state.primary_long = M5.BtnA.wasHold();
    state.secondary_long = M5.BtnB.wasHold();
    state.primary_short =
        M5.BtnA.wasClicked() && !M5.BtnA.wasReleasedAfterHold();
    state.secondary_short =
        M5.BtnB.wasClicked() && !M5.BtnB.wasReleasedAfterHold();
    return state;
}

PowerStatus Board::power_status() const
{
    PowerStatus status;
    status.voltage_mv = M5.Power.getBatteryVoltage();
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

void Board::wake_display()
{
    M5.Display.wakeup();
}

void Board::clear_screen()
{
    M5.Display.fillScreen(TFT_BLACK);
}

void Board::fill_circle(
    std::int16_t x,
    std::int16_t y,
    std::int16_t radius,
    DisplayColor color)
{
    M5.Display.fillCircle(x, y, radius, to_display_color(color));
}

void Board::draw_line(
    std::int16_t x0,
    std::int16_t y0,
    std::int16_t x1,
    std::int16_t y1,
    DisplayColor color)
{
    M5.Display.drawLine(x0, y0, x1, y1, to_display_color(color));
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
    const std::uint32_t foreground_color = to_display_color(foreground);
    const std::uint32_t background_color = to_display_color(background);

    display.fillRect(x, y, width, height, background_color);
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

    const std::uint32_t foreground_color = to_display_color(foreground);
    const std::uint32_t background_color = to_display_color(background);

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
    M5.Display.fillScreen(to_display_color(DisplayColor::Navy));
    draw_polish_ui_text_region(
        4,
        58,
        232,
        16,
        kPolishFontSanityText,
        1,
        DisplayColor::Ivory,
        DisplayColor::Navy);
}

void Board::draw_screen(const char* title, const char* body)
{
    auto& display = M5.Display;
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(4, 3);
    display.setTextSize(2);
    display.println(title);

    display.setCursor(4, 22);
    display.setTextSize(1);
    display.print(body);
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
