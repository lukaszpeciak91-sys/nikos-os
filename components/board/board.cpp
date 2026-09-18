#include "board/board.hpp"

#include "M5Unified.h"
#include "esp_log.h"

namespace {

constexpr char kTag[] = "board";
constexpr std::uint32_t kHoldThresholdMs = 600;

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
    state.a_short = M5.BtnA.wasClicked();
    state.a_long = M5.BtnA.wasHold();
    state.b_short = M5.BtnB.wasClicked();
    state.b_long = M5.BtnB.wasHold();
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
