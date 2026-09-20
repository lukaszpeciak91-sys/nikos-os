#pragma once

#include <cstdint>

#include "board/board.hpp"

namespace nikos::launcher {

enum class Action : std::uint8_t {
    None,
    OpenCommunicator,
    OpenRadioLab,
};

class Launcher final {
public:
    explicit Launcher(board::Board& board);

    void show_splash();
    void begin();
    Action update();

private:
    void update_battery_sample(std::uint32_t now_ms);
    void render();
    void render_battery_if_changed();

    board::Board& board_;
    std::uint8_t selected_index_ = 0;

    bool battery_sample_valid_ = false;
    std::uint32_t last_battery_sample_ms_ = 0;
    std::int32_t cached_battery_percent_ = -1;
    bool rendered_battery_valid_ = false;
    std::int32_t rendered_battery_percent_ = -2;
};

}  // namespace nikos::launcher
