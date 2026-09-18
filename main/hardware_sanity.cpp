#include "hardware_sanity.hpp"

#include <cstdio>

#include "M5Unified.h"
#include "esp_app_desc.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"

namespace {

constexpr char kTag[] = "hardware_sanity";
constexpr std::uint32_t kRefreshIntervalMs = 1000;
constexpr float kBuzzerFrequencyHz = 4000.0F;
constexpr std::uint32_t kBuzzerDurationMs = 120;

}  // namespace

namespace nikos {

void HardwareSanity::begin()
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

    read_device_identity();

    buzzer_command_accepted_ =
        M5.Speaker.tone(kBuzzerFrequencyHz, kBuzzerDurationMs);

    ESP_LOGI(kTag, "Firmware version: %s", app_version_);
    ESP_LOGI(kTag, "Wi-Fi STA MAC: %s", mac_address_);
    ESP_LOGI(
        kTag,
        "M5Unified board id: %d (%s)",
        static_cast<int>(M5.getBoard()),
        board_name());
    ESP_LOGI(
        kTag,
        "Startup buzzer command: %s",
        buzzer_command_accepted_ ? "accepted" : "rejected");

    render();
    last_render_ms_ = m5::millis();
}

void HardwareSanity::update()
{
    M5.update();

    bool redraw = false;

    if (M5.BtnA.wasPressed()) {
        ++button_a_events_;
        last_button_ = "A";
        redraw = true;
        ESP_LOGI(kTag, "Button A event count: %lu",
                 static_cast<unsigned long>(button_a_events_));
    }

    if (M5.BtnB.wasPressed()) {
        ++button_b_events_;
        last_button_ = "B";
        redraw = true;
        ESP_LOGI(kTag, "Button B event count: %lu",
                 static_cast<unsigned long>(button_b_events_));
    }

    const std::uint32_t now_ms = m5::millis();
    if (redraw || (now_ms - last_render_ms_ >= kRefreshIntervalMs)) {
        render();
        last_render_ms_ = now_ms;
    }
}

void HardwareSanity::read_device_identity()
{
    const esp_app_desc_t* app = esp_app_get_description();
    if (app != nullptr) {
        app_version_ = app->version;
    }

    std::uint8_t mac[6] = {};
    const esp_err_t result = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (result == ESP_OK) {
        std::snprintf(
            mac_address_,
            sizeof(mac_address_),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0],
            mac[1],
            mac[2],
            mac[3],
            mac[4],
            mac[5]);
    } else {
        std::snprintf(mac_address_, sizeof(mac_address_), "error:%d", result);
    }
}

const char* HardwareSanity::board_name() const
{
    switch (M5.getBoard()) {
        case m5::board_t::board_M5StickCPlus:
            return "M5StickCPlus";
        default:
            return "unexpected";
    }
}

const char* HardwareSanity::charging_state() const
{
    switch (M5.Power.isCharging()) {
        case m5::Power_Class::is_charging:
            return "charging";
        case m5::Power_Class::is_discharging:
            return "discharging";
        case m5::Power_Class::charge_unknown:
        default:
            return "unknown";
    }
}

void HardwareSanity::render()
{
    const std::int16_t battery_mv = M5.Power.getBatteryVoltage();
    const std::int32_t battery_level = M5.Power.getBatteryLevel();

    auto& display = M5.Display;
    display.fillScreen(TFT_BLACK);
    display.setTextColor(TFT_WHITE, TFT_BLACK);
    display.setCursor(4, 4);
    display.setTextSize(2);
    display.println("Nikos OS sanity");

    display.setTextSize(1);
    display.setCursor(4, 24);
    display.printf("FW: %s\n", app_version_);
    display.printf(
        "Board: %s (%d)\n",
        board_name(),
        static_cast<int>(M5.getBoard()));
    display.printf("MAC: %s\n", mac_address_);

    if (battery_mv >= 0 && battery_level >= 0) {
        display.printf(
            "Battery: %d mV  %ld%%\n",
            battery_mv,
            static_cast<long>(battery_level));
    } else {
        display.println("Battery: unavailable");
    }

    display.printf("Power: %s\n", charging_state());
    display.printf(
        "A events: %lu   B events: %lu\n",
        static_cast<unsigned long>(button_a_events_),
        static_cast<unsigned long>(button_b_events_));
    display.printf("Last button: %s\n", last_button_);
    display.printf(
        "Buzzer cmd: %s\n",
        buzzer_command_accepted_ ? "accepted" : "rejected");
    display.println("Press physical A/B to verify mapping");
}

}  // namespace nikos
