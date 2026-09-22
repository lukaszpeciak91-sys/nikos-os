#pragma once

#include <cstdint>

#include "board/board.hpp"
#include "settings/settings.hpp"
#include "signal_sound/signal_sound_player.hpp"

namespace nikos::launcher {

enum class Action : std::uint8_t {
    None,
    StartCommunicator,
    OpenCommunicator,
    StopCommunicator,
    OpenRadioLab,
    ShutdownRequested,
};

enum class CommunicatorStatus : std::uint8_t {
    Off,
    Searching,
    Ready,
    Available,
};

class Launcher final {
public:
    Launcher(
        board::Board& board,
        settings::State& settings,
        signal_sound::Player& signal_sound);

    void show_splash();
    void begin(CommunicatorStatus communicator_status);
    void begin_tools(CommunicatorStatus communicator_status);
    void set_communicator_status(CommunicatorStatus communicator_status);
    Action update(const board::InputState& input);

private:
    enum class Screen : std::uint8_t {
        Main,
        Tools,
        Entertainment,
        Clock,
        Settings,
        SignalSound,
        Theme,
        EnableCommunicator,
        ActiveCommunicator,
        DisableCommunicator,
        ShutdownConfirm,
    };

    void update_battery_sample(std::uint32_t now_ms);
    void render();
    void render_main();
    void render_tools();
    void render_entertainment();
    void render_clock();
    void render_settings();
    void render_signal_sound();
    void render_theme();
    void render_enable_communicator();
    void render_active_communicator();
    void render_disable_communicator();
    void render_shutdown_confirm();
    void render_battery_if_changed();

    board::Board& board_;
    settings::State& settings_;
    signal_sound::Player& signal_sound_;
    Screen screen_ = Screen::Main;
    CommunicatorStatus communicator_status_ = CommunicatorStatus::Off;
    std::uint8_t selected_index_ = 0;
    std::uint8_t tools_selection_ = 0;
    std::uint8_t settings_selection_ = 0;
    std::uint8_t signal_sound_selection_ = 0;
    std::uint8_t theme_selection_ = 0;
    std::uint8_t active_communicator_selection_ = 0;
    std::uint8_t confirmation_selection_ = 0;

    bool battery_sample_valid_ = false;
    std::uint32_t last_battery_sample_ms_ = 0;
    std::int32_t cached_battery_percent_ = -1;
    bool rendered_battery_valid_ = false;
    std::int32_t rendered_battery_percent_ = -2;
};

}  // namespace nikos::launcher
