#include "board/board.hpp"

#include "polish_ui_font.hpp"

#include "M5Unified.h"
#include "esp_log.h"

namespace {

constexpr char kTag[] = "board";
constexpr std::uint32_t kHoldThresholdMs = 600;

// Theme-varying colors come from ui_theme::Palette. Product-semantic colors
// remain fixed for chromatic themes; Noir deliberately remaps them to
// monochrome presentation at the Board boundary.
constexpr std::uint16_t kStatusActiveRgb565 = 0x8DF5;   // #88BDA8
constexpr std::uint16_t kStatusInactiveRgb565 = 0xB36D; // #B46F6F
constexpr std::uint16_t kAttentionRgb565 = 0xFD20;      // existing SYGNAŁ orange
constexpr std::uint16_t kDangerRgb565 = 0xC2EB;         // #C65F5F

constexpr char kPolishFontSanityText[] =
    "ĄĆĘŁŃÓŚŹŻ ąćęłńóśźż CZEŚĆ! MOŻESZ GADAĆ?";

constexpr std::uint8_t kRtcMonthOffsets[12] = {
    0U, 3U, 2U, 5U, 0U, 3U, 5U, 1U, 4U, 6U, 2U, 4U,
};

constexpr bool rtc_leap_year(std::uint16_t year)
{
    return (year % 4U == 0U)
        && ((year % 100U != 0U) || (year % 400U == 0U));
}

constexpr std::uint8_t rtc_days_in_month(
    std::uint16_t year,
    std::uint8_t month)
{
    switch (month) {
        case 2:
            return rtc_leap_year(year) ? 29U : 28U;
        case 4:
        case 6:
        case 9:
        case 11:
            return 30U;
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            return 31U;
        default:
            return 0U;
    }
}

constexpr bool rtc_date_valid(
    std::uint16_t year,
    std::uint8_t month,
    std::uint8_t day)
{
    const std::uint8_t max_day = rtc_days_in_month(year, month);
    return year >= 1900U
        && year <= 2099U
        && max_day > 0U
        && day >= 1U
        && day <= max_day;
}

// M5Unified rtc_date_t uses 0=Sunday ... 6=Saturday.
constexpr std::uint8_t rtc_weekday_sunday0(
    std::uint16_t year,
    std::uint8_t month,
    std::uint8_t day)
{
    std::uint32_t adjusted_year = year;
    if (month < 3U) {
        --adjusted_year;
    }

    return static_cast<std::uint8_t>(
        (adjusted_year
            + adjusted_year / 4U
            - adjusted_year / 100U
            + adjusted_year / 400U
            + kRtcMonthOffsets[month - 1U]
            + day)
        % 7U);
}

static_assert(rtc_weekday_sunday0(2026U, 10U, 11U) == 0U);
static_assert(rtc_weekday_sunday0(2026U, 10U, 1U) == 4U);
static_assert(rtc_weekday_sunday0(2000U, 1U, 1U) == 6U);

std::int32_t battery_level_from_voltage_mv(std::int16_t voltage_mv)
{
    // Match M5Unified 0.2.22 Power_Class::getBatteryLevel() voltage
    // mapping for AXP192, but reuse the already validated Board voltage
    // snapshot instead of triggering a second independent PMU voltage read.
    const std::int32_t level = static_cast<std::int32_t>(
        (static_cast<float>(voltage_mv) - 3300.0F)
        * 100.0F
        / (4150.0F - 3350.0F));

    return level < 0 ? 0 : (level >= 100 ? 100 : level);
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

    set_display_orientation(DisplayOrientation::Right);
    M5.Display.setBrightness(active_brightness_);
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


bool Board::read_rtc_date(RtcDate& date) const
{
    if (!rtc_available()) {
        return false;
    }

    m5::rtc_date_t rtc_date;
    if (!M5.Rtc.getDate(&rtc_date)
        || rtc_date.year < 0
        || rtc_date.month < 0
        || rtc_date.date < 0) {
        return false;
    }

    const std::uint16_t year =
        static_cast<std::uint16_t>(rtc_date.year);
    const std::uint8_t month =
        static_cast<std::uint8_t>(rtc_date.month);
    const std::uint8_t day =
        static_cast<std::uint8_t>(rtc_date.date);

    if (!rtc_date_valid(year, month, day)) {
        return false;
    }

    date.year = year;
    date.month = month;
    date.day = day;
    return true;
}

bool Board::write_rtc_date(const RtcDate& date)
{
    if (!rtc_available()
        || !rtc_date_valid(date.year, date.month, date.day)) {
        return false;
    }

    const m5::rtc_date_t rtc_date(
        static_cast<std::int16_t>(date.year),
        static_cast<std::int8_t>(date.month),
        static_cast<std::int8_t>(date.day),
        static_cast<std::int8_t>(
            rtc_weekday_sunday0(
                date.year,
                date.month,
                date.day)));
    M5.Rtc.setDate(rtc_date);
    return true;
}

std::int16_t Board::vbus_voltage_mv() const
{
    // Expose the M5Unified/AXP192 hardware reading as-is. Product-level
    // charging presence thresholds belong above Board.
    return M5.Power.getVBUSVoltage();
}

PowerStatus Board::power_status(
    bool include_battery_current) const
{
    PowerStatus status;
    status.current_supported = true;
    status.vbus_voltage_mv = vbus_voltage_mv();

    const std::int16_t voltage_mv = M5.Power.getBatteryVoltage();
    if (voltage_mv <= 0) {
        // AXP192 register-read failures may surface as 0 mV. Keep the whole
        // battery sample explicitly invalid so downstream policy can never
        // mistake an I2C failure for a deeply discharged battery.
        return status;
    }

    status.voltage_mv = voltage_mv;
    status.level_percent =
        battery_level_from_voltage_mv(voltage_mv);
    if (include_battery_current) {
        // AXP192 battery current is diagnostic only. Preserve M5Unified's
        // signed mA value exactly: positive=charging, negative=discharging,
        // zero neutral. Zero is not an error because it is physically valid
        // near charge termination.
        status.current_ma = M5.Power.getBatteryCurrent();
    }

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
    if (notification_volume_level_ == 0) return;
    if (!M5.Speaker.tone(frequency_hz, duration_ms)) {
        ESP_LOGW(kTag, "Buzzer tone request was not accepted");
    }
}

void Board::set_notification_volume_level(std::uint8_t level)
{
    // 100% retains the previous M5Unified master volume of 64.
    // Presets are relative UI steps, not calibrated acoustic percentages.
    constexpr std::uint8_t kVolumes[5] = {0, 16, 32, 48, 64};
    if (level > 4) return;
    notification_volume_level_ = level;
    M5.Speaker.setVolume(kVolumes[level]);
}

void Board::stop_tone()
{
    M5.Speaker.stop();
}

bool Board::begin_microphone()
{
    M5.Speaker.stop();
    M5.Speaker.end();
    // Board::begin keeps internal_mic disabled at boot to preserve normal
    // buzzer behavior. Configure the verified PDM pins only for this tool.
    auto mic_config = M5.Mic.config();
    mic_config.pin_data_in = 34;
    mic_config.pin_ws = 0;
    mic_config.sample_rate = 17000;
    M5.Mic.config(mic_config);
    if (M5.Mic.begin()) return true;
    ESP_LOGE(kTag, "Microphone initialization failed");
    M5.Mic.end();
    if (!M5.Speaker.begin()) ESP_LOGE(kTag, "Speaker recovery failed");
    set_notification_volume_level(notification_volume_level_);
    return false;
}

bool Board::record_microphone(std::int16_t* buffer, std::size_t count, std::uint32_t hz)
{
    return M5.Mic.isRunning() && M5.Mic.record(buffer, count, hz);
}

bool Board::microphone_recording() const
{
    return M5.Mic.isRecording();
}

void Board::end_microphone()
{
    M5.Mic.end();
    if (!M5.Speaker.begin()) {
        ESP_LOGE(kTag, "Speaker recovery after mic failed");
    }
    set_notification_volume_level(notification_volume_level_);
}

void Board::wake_display()
{
    // M5GFX wakeup() restores its remembered brightness. For normal UI,
    // always reapply the configured Active level so a prior dim state cannot
    // survive wake. A Flashlight override keeps owning physical brightness.
    M5.Display.wakeup();
    apply_active_display_brightness();
}

void Board::apply_active_display_brightness()
{
    if (!display_brightness_override_active_) {
        M5.Display.setBrightness(active_brightness_);
    }
}

void Board::dim_display()
{
    if (!display_brightness_override_active_) {
        M5.Display.setBrightness(dimmed_brightness_);
    }
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

void Board::set_display_brightness_profile(
    std::uint8_t active,
    std::uint8_t dimmed)
{
    active_brightness_ = active;
    dimmed_brightness_ = dimmed;
}

void Board::set_display_brightness_override(
    std::uint8_t brightness)
{
    // Temporary presentation-only override. Do not mutate the configured
    // Active/Dimmed profile owned by Settings.
    display_brightness_override_active_ = true;
    M5.Display.setBrightness(brightness);
}

void Board::restore_display_brightness()
{
    display_brightness_override_active_ = false;
    M5.Display.setBrightness(active_brightness_);
}

void Board::fill_flashlight_white()
{
    // Flashlight is a physical light source, not theme UI.
    M5.Display.fillScreen(static_cast<std::uint16_t>(0xFFFF));
}

void Board::clear_screen()
{
    M5.Display.fillScreen(resolve_display_color(DisplayColor::Background));
}

void Board::fill_rect(
    std::int16_t x,
    std::int16_t y,
    std::int16_t width,
    std::int16_t height,
    DisplayColor color)
{
    M5.Display.fillRect(x, y, width, height, resolve_display_color(color));
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
    if (text_delegate_ != nullptr && text_size >= 2
        && text_delegate_(text_context_, x, y, width, height, text,
                          text_size, foreground, background)) {
        return;
    }
    auto& display = M5.Display;
    const std::uint16_t foreground_color = resolve_display_color(foreground);
    const std::uint16_t background_color = resolve_display_color(background);

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

std::int32_t Board::polish_ui_text_width(const char* utf8_text) const
{
    auto& display = M5.Display;
    const lgfx::IFont* previous_font = display.getFont();
    const lgfx::TextStyle previous_style = display.getTextStyle();
    display.setFont(&detail::kPolishUiFont);
    display.setTextSize(1);
    const std::int32_t width = display.textWidth(utf8_text == nullptr ? "" : utf8_text);
    display.setFont(previous_font);
    display.setTextStyle(previous_style);
    return width;
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

    const std::uint16_t foreground_color = resolve_display_color(foreground);
    const std::uint16_t background_color = resolve_display_color(background);

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
    const std::uint16_t background =
        resolve_display_color(DisplayColor::Background);
    const std::uint16_t foreground =
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

std::uint16_t Board::resolve_display_color(DisplayColor color) const
{
    const ui_theme::Palette& palette = ui_theme::palette(theme_);
    const bool noir = theme_ == ui_theme::Theme::Noir;

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
            return noir ? palette.accent : kStatusActiveRgb565;
        case DisplayColor::StatusInactive:
            return noir ? palette.secondary_text : kStatusInactiveRgb565;
        case DisplayColor::Attention:
            return noir ? palette.accent : kAttentionRgb565;
        case DisplayColor::Danger:
            return noir ? palette.primary_text : kDangerRgb565;
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
