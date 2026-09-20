#pragma once

#include <cstdint>

#include "board/board.hpp"

namespace nikos::launcher {

enum class Action : std::uint8_t {
    None,
    StartCommunicator,
    OpenCommunicator,
    StopCommunicator,
    OpenRadioLab,
    ShutdownRequested,
};

class Launcher final {
public:
    explicit Launcher(board::Board& board);

    void show_splash();
    void begin(bool communicator_active);
    Action update();

private:
    enum class Screen : std::uint8_t {
        Main,
        EnableCommunicator,
        ActiveCommunicator,
        ShutdownConfirm,
    };

    void update_battery_sample(std::uint32_t now_ms);
    void render();
    void render_main();
    void render_enable_communicator();
    void render_active_communicator();
    void render_shutdown_confirm();
    void render_battery_if_changed();

    board::Board& board_;
    Screen screen_ = Screen::Main;
    bool communicator_active_ = false;
    std::uint8_t selected_index_ = 0;
    std::uint8_t active_communicator_selection_ = 0;
    std::uint8_t shutdown_selection_ = 0;

    bool battery_sample_valid_ = false;
    std::uint32_t last_battery_sample_ms_ = 0;
    std::int32_t cached_battery_percent_ = -1;
    bool rendered_battery_valid_ = false;
    std::int32_t rendered_battery_percent_ = -2;
};

}  // namespace nikos::launcher
