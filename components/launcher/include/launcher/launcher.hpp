#pragma once

#include <cstdint>

#include "board/board.hpp"
#include "font_core/font_core.hpp"
#include "clock/clock_service.hpp"
#include "countdown/countdown_service.hpp"
#include "settings/settings.hpp"
#include "signal_sound/signal_sound_player.hpp"

namespace nikos::launcher {

enum class Action : std::uint8_t {
    None,
    StartCommunicator,
    OpenCommunicator,
    StopCommunicator,
    OpenRadioLab,
    OpenPowerDiag,
    OpenFlashlight,
    OpenSoundMeter,
    OpenSnake,
    OpenMiniRacer,
    OpenSpaceDefender,
    OpenFacts,
    BrightnessChanged,
    SignalVolumeChanged,
    AutoPowerOffChanged,
    ShutdownRequested,
};

enum class CommunicatorStatus : std::uint8_t {
    Off,
    Searching,
    Ready,
    Available,
};

enum class CommunicatorDeliveryStatus : std::uint8_t {
    None,
    Sending,
    Delivered,
    Failed,
};

class Launcher final {
public:
    Launcher(
        board::Board& board,
        clock::ClockService& clock_service,
        countdown::Service& countdown,
        settings::State& settings,
        signal_sound::Player& signal_sound);

    void show_splash();
    void begin(CommunicatorStatus communicator_status);
    void begin_tools(CommunicatorStatus communicator_status);
    void begin_entertainment(CommunicatorStatus communicator_status);
    void set_communicator_status(
        CommunicatorStatus communicator_status,
        bool render_if_changed = true);
    void set_communicator_delivery_status(
        CommunicatorDeliveryStatus status,
        bool render_if_changed = true);
    void restore_auto_power_off_preference(bool enabled);
    void restore_signal_volume_preference(std::uint8_t level);
    void redraw();
    bool showing_communicator_ui() const;
    Action update(const board::InputState& input);
    bool take_entertainment_launch() {
        const bool value=entertainment_launch_;
        entertainment_launch_=false;
        return value;
    }

private:
    bool entertainment_launch_=false;
    enum class StopwatchState : std::uint8_t {
        Idle,
        Running,
        Stopped,
    };

    enum class Screen : std::uint8_t {
        Main,
        Tools,
        Entertainment,
        Clock,
        TimerSetup,
        TimerActive,
        Stopwatch,
        ClockSetHour,
        ClockSetMinute,
        ClockSetFailed,
        ClockSetDateDay,
        ClockSetDateMonth,
        ClockSetDateYear,
        ClockDateSetFailed,
        ClockDateNeedsTime,
        Calendar,
        Settings,
        SoundMenu,
        SignalSound,
        SignalVolume,
        Brightness,
        Theme,
        Orientation,
        AutoPowerOff,
        EnableCommunicator,
        ActiveCommunicator,
        DisableCommunicator,
        ShutdownConfirm,
    };

    void update_battery_sample(std::uint32_t now_ms, bool force = false);
    void update_clock_sample(std::uint32_t now_ms, bool force = false);
    void reset_stopwatch_session();
    void exit_stopwatch_to_clock();
    void update_clock_viewport();
    void begin_date_editor();
    void clamp_edit_date_day();
    void begin_calendar();
    void move_calendar_month(bool forward);
    std::uint64_t stopwatch_elapsed_us() const;
    std::uint32_t stopwatch_display_seconds() const;
    void render();
    void render_main();
    void render_tools();
    void render_entertainment();
    void render_clock();
    void render_timer_setup();
    void render_timer_active();
    void render_stopwatch();
    void render_clock_editor(bool editing_hour);
    void render_clock_set_failed();
    void render_date_editor();
    void render_date_set_failed();
    void render_date_needs_time();
    void render_calendar();
    void render_header_time_if_changed();
    void render_clock_time_if_changed();
    void render_clock_timer_if_changed();
    void render_timer_countdown_if_changed();
    void render_stopwatch_time_if_changed();
    void render_settings();
    void draw_pilot_text(std::int16_t x, std::int16_t y,
        std::int16_t width, std::int16_t height, const char* text,
        std::uint8_t standard_size, board::DisplayColor fg,
        board::DisplayColor bg);
    void render_signal_sound();
    void render_sound_menu();
    void render_signal_volume();
    void render_brightness();
    void render_theme();
    void render_orientation();
    void render_auto_power_off();
    void render_enable_communicator();
    void render_active_communicator();
    void render_disable_communicator();
    void render_shutdown_confirm();
    void render_communicator_delivery_status();
    void render_battery_if_changed();

    board::Board& board_;
    font_core::Renderer font_renderer_;
    clock::ClockService& clock_service_;
    countdown::Service& countdown_;
    settings::State& settings_;
    signal_sound::Player& signal_sound_;
    Screen screen_ = Screen::Main;
    CommunicatorStatus communicator_status_ = CommunicatorStatus::Off;
    CommunicatorDeliveryStatus communicator_delivery_status_ =
        CommunicatorDeliveryStatus::None;
    std::uint8_t selected_index_ = 0;
    std::uint8_t tools_selection_ = 0;
    std::uint8_t entertainment_selection_ = 0;
    std::uint8_t clock_selection_ = 0;
    std::uint8_t clock_view_start_ = 0;
    std::uint8_t timer_selection_ = 0;
    StopwatchState stopwatch_state_ = StopwatchState::Idle;
    std::uint8_t stopwatch_selection_ = 0;
    std::uint64_t stopwatch_run_started_us_ = 0;
    std::uint64_t stopwatch_accumulated_us_ = 0;
    bool rendered_stopwatch_time_valid_ = false;
    std::uint32_t rendered_stopwatch_seconds_ = 0;
    std::uint8_t edit_hour_ = 0;
    std::uint8_t edit_minute_ = 0;
    std::uint8_t edit_day_ = 1;
    std::uint8_t edit_month_ = 1;
    std::uint16_t edit_year_ = 2026;
    bool calendar_initialized_ = false;
    std::uint16_t calendar_year_ = 2026;
    std::uint8_t calendar_month_ = 1;
    std::uint8_t calendar_action_selection_ = 0;
    std::uint8_t settings_selection_ = 0;
    std::uint8_t signal_sound_selection_ = 0;
    std::uint8_t sound_menu_selection_ = 0;
    std::uint8_t signal_volume_selection_ = 4;
    std::uint8_t brightness_selection_ = 0;
    std::uint8_t theme_selection_ = 0;
    std::uint8_t orientation_selection_ = 0;
    std::uint8_t auto_power_off_selection_ = 0;
    std::uint8_t active_communicator_selection_ = 0;
    std::uint8_t confirmation_selection_ = 0;

    bool clock_sample_valid_ = false;
    std::uint32_t last_clock_sample_ms_ = 0;
    clock::Reading cached_clock_{};
    char cached_clock_text_[6] = "--:--";
    char rendered_header_clock_text_[6] = "";
    char rendered_clock_screen_text_[6] = "";
    bool rendered_clock_timer_valid_ = false;
    countdown::State rendered_clock_timer_state_ = countdown::State::Idle;
    std::uint32_t rendered_clock_timer_seconds_ = 0;
    bool rendered_timer_countdown_valid_ = false;
    countdown::State rendered_timer_countdown_state_ = countdown::State::Idle;
    std::uint32_t rendered_timer_seconds_ = 0;

    bool battery_sample_valid_ = false;
    std::uint32_t last_battery_sample_ms_ = 0;
    std::int32_t cached_battery_percent_ = -1;
    bool rendered_battery_valid_ = false;
    std::int32_t rendered_battery_percent_ = -2;
};

}  // namespace nikos::launcher
