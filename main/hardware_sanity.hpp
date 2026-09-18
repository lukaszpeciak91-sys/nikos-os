#pragma once

#include <cstdint>

namespace nikos {

class HardwareSanity final {
public:
    void begin();
    void update();

private:
    void read_device_identity();
    void render();
    const char* board_name() const;
    const char* charging_state() const;

    std::uint32_t button_a_events_ = 0;
    std::uint32_t button_b_events_ = 0;
    std::uint32_t last_render_ms_ = 0;
    const char* last_button_ = "none";
    const char* app_version_ = "unknown";
    bool buzzer_command_accepted_ = false;
    char mac_address_[18] = "unavailable";
};

}  // namespace nikos
