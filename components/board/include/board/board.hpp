#pragma once

#include <cstdint>

namespace nikos::board {

enum class ChargeState : std::uint8_t {
    Unknown,
    Charging,
    Discharging,
};

struct PowerStatus {
    std::int16_t voltage_mv = -1;
    std::int32_t level_percent = -1;
    ChargeState charge_state = ChargeState::Unknown;
};

struct InputState {
    bool a_short = false;
    bool a_long = false;
    bool b_short = false;
    bool b_long = false;
};

class Board final {
public:
    bool begin();
    InputState poll_input();

    PowerStatus power_status() const;
    void tone(float frequency_hz, std::uint32_t duration_ms);
    void draw_screen(const char* title, const char* body);
    const char* detected_board_name() const;
};

const char* charge_state_name(ChargeState state);

}  // namespace nikos::board
