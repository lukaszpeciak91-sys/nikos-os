#include "launcher/launcher.hpp"

#include <array>
#include <cstdio>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

struct Entry {
    const char* label;
};

constexpr std::array<Entry, 6> kEntries = {{
    {"Komunikator"},
    {"Narzedzia"},
    {"Rozrywka"},
    {"Zegar"},
    {"Ustawienia"},
    {"Wylacz"},
}};

constexpr std::size_t kVisibleLauncherRows = 4;

constexpr std::uint32_t kSignalFrameMs = 110;
constexpr std::uint32_t kSyncFrameMs = 140;
constexpr std::uint32_t kFormFrameMs = 170;
constexpr std::uint32_t kBatterySampleIntervalMs = 1000;
constexpr std::uint32_t kClockSampleIntervalMs = 1000;

std::uint8_t signal_sound_index(nikos::settings::SignalSound sound)
{
    switch (sound) {
        case nikos::settings::SignalSound::Classic:
            return 1;
        case nikos::settings::SignalSound::Pager:
            return 2;
        case nikos::settings::SignalSound::Gentle:
        default:
            return 0;
    }
}

nikos::settings::SignalSound signal_sound_from_index(std::uint8_t index)
{
    switch (index) {
        case 1:
            return nikos::settings::SignalSound::Classic;
        case 2:
            return nikos::settings::SignalSound::Pager;
        case 0:
        default:
            return nikos::settings::SignalSound::Gentle;
    }
}

void format_mmss(
    std::uint32_t total_seconds,
    char* output,
    std::size_t output_size)
{
    std::snprintf(
        output,
        output_size,
        "%02lu:%02lu",
        static_cast<unsigned long>(total_seconds / 60U),
        static_cast<unsigned long>(total_seconds % 60U));
}

std::uint8_t theme_index(nikos::ui_theme::Theme theme)
{
    switch (theme) {
        case nikos::ui_theme::Theme::Bursztyn:
            return 1;
        case nikos::ui_theme::Theme::Graphite:
            return 2;
        case nikos::ui_theme::Theme::Lava:
            return 3;
        case nikos::ui_theme::Theme::Matrix:
            return 4;
        case nikos::ui_theme::Theme::Nikos:
        default:
            return 0;
    }
}

nikos::ui_theme::Theme theme_from_index(std::uint8_t index)
{
    switch (index) {
        case 1:
            return nikos::ui_theme::Theme::Bursztyn;
        case 2:
            return nikos::ui_theme::Theme::Graphite;
        case 3:
            return nikos::ui_theme::Theme::Lava;
        case 4:
            return nikos::ui_theme::Theme::Matrix;
        case 0:
        default:
            return nikos::ui_theme::Theme::Nikos;
    }
}

std::uint32_t now_ms()
{
    return static_cast<std::uint32_t>(
        pdTICKS_TO_MS(xTaskGetTickCount()));
}

void clear_shell(nikos::board::Board& board)
{
    board.draw_text_region(
        0,
        0,
        240,
        135,
        "",
        1,
        nikos::board::DisplayColor::PrimaryText,
        nikos::board::DisplayColor::Background);
}

void clear_logo_area(nikos::board::Board& board)
{
    board.draw_text_region(
        18,
        28,
        208,
        74,
        "",
        1,
        nikos::board::DisplayColor::PrimaryText,
        nikos::board::DisplayColor::Background);
}

void draw_fragment(
    nikos::board::Board& board,
    std::int16_t x,
    std::int16_t y,
    std::int16_t width,
    nikos::board::DisplayColor color)
{
    board.draw_line(x, y, static_cast<std::int16_t>(x + width), y, color);
    board.draw_line(
        x,
        static_cast<std::int16_t>(y + 1),
        static_cast<std::int16_t>(x + width),
        static_cast<std::int16_t>(y + 1),
        color);
}

void draw_sliced_logo(nikos::board::Board& board, bool show_os)
{
    clear_logo_area(board);

    board.draw_text_region(
        39,
        34,
        174,
        46,
        "NIKOS",
        4,
        nikos::board::DisplayColor::PrimaryText,
        nikos::board::DisplayColor::Background);

    for (std::int16_t y : {58, 64, 70}) {
        board.draw_line(
            39,
            y,
            184,
            y,
            nikos::board::DisplayColor::Background);
        board.draw_line(
            39,
            static_cast<std::int16_t>(y + 1),
            184,
            static_cast<std::int16_t>(y + 1),
            nikos::board::DisplayColor::Background);
    }

    if (show_os) {
        board.draw_text_region(
            173,
            78,
            52,
            24,
            "OS",
            2,
            nikos::board::DisplayColor::Accent,
            nikos::board::DisplayColor::Background);
    }
}

}  // namespace

namespace nikos::launcher {

Launcher::Launcher(
    board::Board& board,
    clock::ClockService& clock_service,
    countdown::Service& countdown,
    settings::State& settings,
    signal_sound::Player& signal_sound)
    : board_(board),
      clock_service_(clock_service),
      countdown_(countdown),
      settings_(settings),
      signal_sound_(signal_sound)
{
}

void Launcher::show_splash()
{
    clear_shell(board_);

    // SIGNAL: sparse fragments arrive first.
    draw_fragment(board_, 30, 61, 18, board::DisplayColor::PrimaryText);
    draw_fragment(board_, 58, 48, 12, board::DisplayColor::SecondaryText);
    draw_fragment(board_, 78, 70, 19, board::DisplayColor::PrimaryText);
    draw_fragment(board_, 108, 55, 14, board::DisplayColor::SecondaryText);
    draw_fragment(board_, 137, 67, 22, board::DisplayColor::PrimaryText);
    draw_fragment(board_, 171, 51, 16, board::DisplayColor::SecondaryText);
    draw_fragment(board_, 194, 73, 18, board::DisplayColor::PrimaryText);
    vTaskDelay(pdMS_TO_TICKS(kSignalFrameMs));

    // SYNCHRONIZING: fragments settle onto shared scan bands.
    clear_logo_area(board_);
    for (std::int16_t x : {40, 70, 100, 130, 160}) {
        draw_fragment(board_, x, 51, 20, board::DisplayColor::SecondaryText);
        draw_fragment(board_, x, 61, 24, board::DisplayColor::PrimaryText);
        draw_fragment(board_, x, 71, 18, board::DisplayColor::SecondaryText);
    }
    draw_fragment(board_, 55, 81, 13, board::DisplayColor::PrimaryText);
    draw_fragment(board_, 151, 81, 16, board::DisplayColor::PrimaryText);
    vTaskDelay(pdMS_TO_TICKS(kSyncFrameMs));

    // FORMING: the wordmark resolves before the secondary OS mark appears.
    draw_sliced_logo(board_, false);
    vTaskDelay(pdMS_TO_TICKS(kFormFrameMs));

    // READY: final clean branded mark. app_main keeps this frame visible ~750 ms.
    draw_sliced_logo(board_, true);
}

void Launcher::begin(CommunicatorStatus communicator_status)
{
    screen_ = Screen::Main;
    communicator_status_ = communicator_status;
    selected_index_ = 0;
    tools_selection_ = 0;
    clock_selection_ = 0;
    timer_selection_ = 0;
    settings_selection_ = 0;
    signal_sound_selection_ = signal_sound_index(settings_.signal_sound);
    theme_selection_ = theme_index(settings_.theme);
    active_communicator_selection_ = 0;

    const std::uint32_t now = now_ms();
    update_clock_sample(now, true);
    update_battery_sample(now);
    render();
}

void Launcher::begin_tools(CommunicatorStatus communicator_status)
{
    screen_ = Screen::Tools;
    communicator_status_ = communicator_status;
    selected_index_ = 1;
    tools_selection_ = 0;

    const std::uint32_t now = now_ms();
    update_clock_sample(now, true);
    update_battery_sample(now);
    render();
}

void Launcher::redraw()
{
    if (screen_ == Screen::TimerActive
        && countdown_.state() == countdown::State::Idle) {
        screen_ = Screen::TimerSetup;
        timer_selection_ = 0;
    }

    const std::uint32_t now = now_ms();
    update_clock_sample(now, true);
    update_battery_sample(now);
    render();
}

void Launcher::set_communicator_status(CommunicatorStatus communicator_status)
{
    if (communicator_status_ == communicator_status) {
        return;
    }
    communicator_status_ = communicator_status;
    if (screen_ == Screen::Main) {
        render();
    }
}

Action Launcher::update(const board::InputState& input)
{
    const std::uint32_t now = now_ms();
    update_clock_sample(now);
    update_battery_sample(now);

    if (screen_ == Screen::Main) {
        render_header_time_if_changed();
        render_battery_if_changed();
    } else if (screen_ == Screen::Clock) {
        render_clock_time_if_changed();
        render_clock_timer_if_changed();
    } else if (screen_ == Screen::TimerActive) {
        render_timer_countdown_if_changed();
    }

    if (screen_ == Screen::EnableCommunicator) {
        if (input.secondary_short) {
            confirmation_selection_ ^= 1U;
            render();
            return Action::None;
        }
        if (input.primary_short) {
            if (confirmation_selection_ == 0U) {
                return Action::StartCommunicator;
            }
            screen_ = Screen::Main;
            render();
        }
        if (input.secondary_long) {
            screen_ = Screen::Main;
            render();
        }

        return Action::None;
    }

    if (screen_ == Screen::ActiveCommunicator) {
        if (input.secondary_short) {
            active_communicator_selection_ =
                static_cast<std::uint8_t>(
                    (active_communicator_selection_ + 1U) % 2U);
            render();
            return Action::None;
        }

        if (input.primary_short) {
            if (active_communicator_selection_ == 0) {
                return Action::OpenCommunicator;
            }
            confirmation_selection_ = 1;
            screen_ = Screen::DisableCommunicator;
            render();
        }

        return Action::None;
    }

    if (screen_ == Screen::DisableCommunicator) {
        if (input.secondary_short) {
            confirmation_selection_ ^= 1U;
            render();
            return Action::None;
        }
        if (input.primary_short) {
            if (confirmation_selection_ == 0U) {
                return Action::StopCommunicator;
            }
            screen_ = Screen::ActiveCommunicator;
            render();
        } else if (input.secondary_long) {
            screen_ = Screen::ActiveCommunicator;
            render();
        }
        return Action::None;
    }

    if (screen_ == Screen::Tools) {
        if (input.secondary_long) {
            screen_ = Screen::Main;
            render();
            return Action::None;
        }

        if (input.secondary_short) {
            tools_selection_ =
                static_cast<std::uint8_t>((tools_selection_ + 1U) % 2U);
            render();
            return Action::None;
        }

        if (input.primary_short) {
            if (tools_selection_ == 0) {
                return Action::OpenRadioLab;
            }

            screen_ = Screen::Main;
            render();
        }

        return Action::None;
    }

    if (screen_ == Screen::Entertainment) {
        if (input.secondary_long || input.primary_short) {
            screen_ = Screen::Main;
            render();
            return Action::None;
        }

        if (input.secondary_short) {
            render();
        }

        return Action::None;
    }

    if (screen_ == Screen::Clock) {
        if (input.secondary_long) {
            screen_ = Screen::Main;
            render();
            return Action::None;
        }

        if (input.secondary_short) {
            clock_selection_ =
                static_cast<std::uint8_t>((clock_selection_ + 1U) % 3U);
            render();
            return Action::None;
        }

        if (input.primary_short) {
            if (clock_selection_ == 0U) {
                const clock::Reading reading = clock_service_.read();
                edit_hour_ = reading.valid ? reading.time.hour : 0U;
                edit_minute_ = reading.valid ? reading.time.minute : 0U;
                screen_ = Screen::ClockSetHour;
            } else if (clock_selection_ == 1U) {
                timer_selection_ = 0;
                screen_ = countdown_.state() == countdown::State::Idle
                    ? Screen::TimerSetup
                    : Screen::TimerActive;
            } else {
                screen_ = Screen::Main;
            }
            render();
        }

        return Action::None;
    }

    if (screen_ == Screen::TimerSetup) {
        if (input.secondary_long) {
            clock_selection_ = 1;
            screen_ = Screen::Clock;
            render();
            return Action::None;
        }

        if (input.secondary_short) {
            countdown_.advance_configured_duration();
            render();
            return Action::None;
        }

        if (input.primary_short && countdown_.start()) {
            timer_selection_ = 0;
            screen_ = Screen::TimerActive;
            render();
        }

        return Action::None;
    }

    if (screen_ == Screen::TimerActive) {
        if (countdown_.state() == countdown::State::Idle) {
            timer_selection_ = 0;
            screen_ = Screen::TimerSetup;
            render();
            return Action::None;
        }

        if (input.secondary_long) {
            clock_selection_ = 1;
            screen_ = Screen::Clock;
            render();
            return Action::None;
        }

        if (input.secondary_short) {
            timer_selection_ =
                static_cast<std::uint8_t>((timer_selection_ + 1U) % 3U);
            render();
            return Action::None;
        }

        if (!input.primary_short) {
            return Action::None;
        }

        if (timer_selection_ == 0U) {
            if (countdown_.state() == countdown::State::Running) {
                countdown_.pause();
            } else if (countdown_.state() == countdown::State::Paused) {
                countdown_.resume();
            }
            render();
        } else if (timer_selection_ == 1U) {
            countdown_.reset();
            timer_selection_ = 0;
            screen_ = Screen::TimerSetup;
            render();
        } else {
            clock_selection_ = 1;
            screen_ = Screen::Clock;
            render();
        }

        return Action::None;
    }

    if (screen_ == Screen::ClockSetHour
        || screen_ == Screen::ClockSetMinute) {
        if (input.secondary_long) {
            screen_ = Screen::Clock;
            render();
            return Action::None;
        }

        if (input.secondary_short) {
            if (screen_ == Screen::ClockSetHour) {
                edit_hour_ = static_cast<std::uint8_t>((edit_hour_ + 1U) % 24U);
            } else {
                edit_minute_ = static_cast<std::uint8_t>((edit_minute_ + 1U) % 60U);
            }
            render();
            return Action::None;
        }

        if (!input.primary_short) {
            return Action::None;
        }

        if (screen_ == Screen::ClockSetHour) {
            screen_ = Screen::ClockSetMinute;
            render();
            return Action::None;
        }

        if (clock_service_.set_time(edit_hour_, edit_minute_)) {
            update_clock_sample(now, true);
            clock_selection_ = 0;
            screen_ = Screen::Clock;
        } else {
            update_clock_sample(now, true);
            screen_ = Screen::ClockSetFailed;
        }
        render();
        return Action::None;
    }

    if (screen_ == Screen::ClockSetFailed) {
        if (input.primary_short
            || input.secondary_short
            || input.secondary_long) {
            screen_ = Screen::Clock;
            render();
        }
        return Action::None;
    }

    if (screen_ == Screen::Settings) {
        if (input.secondary_long) {
            signal_sound_.stop();
            screen_ = Screen::Main;
            render();
            return Action::None;
        }

        if (input.secondary_short) {
            settings_selection_ =
                static_cast<std::uint8_t>((settings_selection_ + 1U) % 3U);
            render();
            return Action::None;
        }

        if (input.primary_short) {
            if (settings_selection_ == 0) {
                signal_sound_selection_ =
                    signal_sound_index(settings_.signal_sound);
                screen_ = Screen::SignalSound;
            } else if (settings_selection_ == 1) {
                theme_selection_ = theme_index(settings_.theme);
                screen_ = Screen::Theme;
            } else {
                signal_sound_.stop();
                screen_ = Screen::Main;
            }

            render();
        }

        return Action::None;
    }

    if (screen_ == Screen::SignalSound) {
        if (input.secondary_long) {
            signal_sound_.stop();
            screen_ = Screen::Settings;
            render();
            return Action::None;
        }

        if (input.secondary_short) {
            signal_sound_selection_ =
                static_cast<std::uint8_t>(
                    (signal_sound_selection_ + 1U) % 4U);
            render();
            return Action::None;
        }

        if (!input.primary_short) {
            return Action::None;
        }

        if (signal_sound_selection_ == 3U) {
            signal_sound_.stop();
            settings_selection_ = 0;
            screen_ = Screen::Settings;
            render();
            return Action::None;
        }

        settings_.signal_sound =
            signal_sound_from_index(signal_sound_selection_);
        signal_sound_.stop();
        signal_sound_.play_selected();
        render();
        return Action::None;
    }

    if (screen_ == Screen::Theme) {
        if (input.secondary_long) {
            screen_ = Screen::Settings;
            render();
            return Action::None;
        }

        if (input.secondary_short) {
            theme_selection_ =
                static_cast<std::uint8_t>((theme_selection_ + 1U) % 6U);
            render();
            return Action::None;
        }

        if (!input.primary_short) {
            return Action::None;
        }

        if (theme_selection_ == 5U) {
            settings_selection_ = 1;
            screen_ = Screen::Settings;
            render();
            return Action::None;
        }

        settings_.theme = theme_from_index(theme_selection_);
        board_.set_theme(settings_.theme);
        render();
        return Action::None;
    }

    if (screen_ == Screen::ShutdownConfirm) {
        if (input.secondary_short) {
            confirmation_selection_ ^= 1U;
            render();
            return Action::None;
        }
        if (input.primary_short) {
            if (confirmation_selection_ == 0U) {
                return Action::ShutdownRequested;
            }
            screen_ = Screen::Main;
            render();
        }
        if (input.secondary_long) {
            screen_ = Screen::Main;
            render();
        }

        return Action::None;
    }

    if (input.secondary_long) {
        return Action::None;
    }

    if (input.secondary_short) {
        selected_index_ =
            static_cast<std::uint8_t>((selected_index_ + 1U) % kEntries.size());
        render();
        return Action::None;
    }

    if (!input.primary_short) {
        return Action::None;
    }

    switch (selected_index_) {
        case 0:
            active_communicator_selection_ = 0;
            confirmation_selection_ = 0;
            screen_ = communicator_status_ != CommunicatorStatus::Off
                ? Screen::ActiveCommunicator
                : Screen::EnableCommunicator;
            break;
        case 1:
            tools_selection_ = 0;
            screen_ = Screen::Tools;
            break;
        case 2:
            screen_ = Screen::Entertainment;
            break;
        case 3:
            clock_selection_ = 0;
            screen_ = Screen::Clock;
            break;
        case 4:
            settings_selection_ = 0;
            screen_ = Screen::Settings;
            break;
        case 5:
            confirmation_selection_ = 1;
            screen_ = Screen::ShutdownConfirm;
            break;
        default:
            return Action::None;
    }

    render();
    return Action::None;
}

void Launcher::update_battery_sample(std::uint32_t now_ms)
{
    if (battery_sample_valid_
        && now_ms - last_battery_sample_ms_ < kBatterySampleIntervalMs) {
        return;
    }

    const board::PowerStatus power = board_.power_status();
    cached_battery_percent_ =
        power.level_percent >= 0 ? power.level_percent : -1;
    last_battery_sample_ms_ = now_ms;
    battery_sample_valid_ = true;
}

void Launcher::update_clock_sample(
    std::uint32_t now_ms,
    bool force)
{
    if (!force
        && clock_sample_valid_
        && now_ms - last_clock_sample_ms_ < kClockSampleIntervalMs) {
        return;
    }

    cached_clock_ = clock_service_.read();
    clock::ClockService::format_hhmm(
        cached_clock_,
        cached_clock_text_,
        sizeof(cached_clock_text_));
    last_clock_sample_ms_ = now_ms;
    clock_sample_valid_ = true;
}


void Launcher::render()
{
    switch (screen_) {
        case Screen::Main:
            render_main();
            break;
        case Screen::Tools:
            render_tools();
            break;
        case Screen::Entertainment:
            render_entertainment();
            break;
        case Screen::Clock:
            render_clock();
            break;
        case Screen::TimerSetup:
            render_timer_setup();
            break;
        case Screen::TimerActive:
            render_timer_active();
            break;
        case Screen::ClockSetHour:
            render_clock_editor(true);
            break;
        case Screen::ClockSetMinute:
            render_clock_editor(false);
            break;
        case Screen::ClockSetFailed:
            render_clock_set_failed();
            break;
        case Screen::Settings:
            render_settings();
            break;
        case Screen::SignalSound:
            render_signal_sound();
            break;
        case Screen::Theme:
            render_theme();
            break;
        case Screen::EnableCommunicator:
            render_enable_communicator();
            break;
        case Screen::ActiveCommunicator:
            render_active_communicator();
            break;
        case Screen::DisableCommunicator:
            render_disable_communicator();
            break;
        case Screen::ShutdownConfirm:
            render_shutdown_confirm();
            break;
    }
}

void Launcher::render_main()
{
    clear_shell(board_);
    rendered_header_clock_text_[0] = '\0';
    rendered_battery_valid_ = false;

    board_.draw_text_region(
        10,
        6,
        60,
        18,
        "NIKOS",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        72,
        6,
        30,
        18,
        "OS",
        2,
        board::DisplayColor::Accent,
        board::DisplayColor::Background);
    board_.draw_line(
        10,
        26,
        229,
        26,
        board::DisplayColor::SecondaryText);

    render_header_time_if_changed();
    render_battery_if_changed();

    std::size_t first_visible = 0;
    if (selected_index_ >= kVisibleLauncherRows) {
        first_visible =
            selected_index_ - kVisibleLauncherRows + 1U;
    }

    for (std::size_t slot = 0; slot < kVisibleLauncherRows; ++slot) {
        const std::size_t index = first_visible + slot;
        if (index >= kEntries.size()) {
            break;
        }

        const bool selected = index == selected_index_;
        const std::int16_t row_y =
            static_cast<std::int16_t>(32 + slot * 19);
        const board::DisplayColor row_background =
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background;

        if (selected) {
            board_.draw_text_region(
                8,
                static_cast<std::int16_t>(row_y - 2),
                224,
                18,
                "",
                1,
                board::DisplayColor::PrimaryText,
                board::DisplayColor::Surface);
            board_.draw_line(
                8,
                static_cast<std::int16_t>(row_y - 1),
                8,
                static_cast<std::int16_t>(row_y + 14),
                board::DisplayColor::Accent);
            board_.draw_line(
                9,
                static_cast<std::int16_t>(row_y - 1),
                9,
                static_cast<std::int16_t>(row_y + 14),
                board::DisplayColor::Accent);
        }

        board_.draw_text_region(
            18,
            row_y,
            index == 0 ? 140 : 184,
            18,
            kEntries[index].label,
            2,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            row_background);

        if (index == 0) {
            const bool active =
                communicator_status_ != CommunicatorStatus::Off;
            const board::DisplayColor indicator_color =
                active
                    ? board::DisplayColor::Accent
                    : board::DisplayColor::SecondaryText;

            board_.fill_circle(
                164,
                static_cast<std::int16_t>(row_y + 7),
                4,
                indicator_color);

            if (!active) {
                board_.fill_circle(
                    164,
                    static_cast<std::int16_t>(row_y + 7),
                    2,
                    row_background);
            }

            const char* status_text = "OFF";
            if (communicator_status_ == CommunicatorStatus::Searching) {
                status_text = "SZUKAM";
            } else if (communicator_status_ == CommunicatorStatus::Ready) {
                status_text = "GOTOWY";
            } else if (communicator_status_ == CommunicatorStatus::Available) {
                status_text = "DOSTEPNY";
            }
            board_.draw_text_region(
                173,
                static_cast<std::int16_t>(row_y + 2),
                58,
                12,
                status_text,
                1,
                communicator_status_ == CommunicatorStatus::Available
                    ? board::DisplayColor::StatusActive
                    : board::DisplayColor::SecondaryText,
                row_background);
        }
    }

    board_.draw_text_region(
        10,
        112,
        220,
        12,
        "M5 OTWORZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void Launcher::render_tools()
{
    clear_shell(board_);

    board_.draw_text_region(
        14,
        14,
        212,
        20,
        "NARZEDZIA",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    constexpr const char* kTools[2] = {
        "RadioLab",
        "Powrot",
    };

    for (std::uint8_t index = 0; index < 2; ++index) {
        const bool selected = index == tools_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(48 + index * 28);

        board_.draw_text_region(
            22,
            y,
            196,
            22,
            kTools[index],
            2,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 17),
                board::DisplayColor::Accent);
        }
    }
    board_.draw_text_region(
        14,
        112,
        212,
        14,
        "M5 WYBIERZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void Launcher::render_entertainment()
{
    clear_shell(board_);

    board_.draw_text_region(
        14,
        18,
        212,
        20,
        "ROZRYWKA",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        22,
        58,
        196,
        22,
        "Powrot",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Surface);
    board_.draw_line(
        14,
        58,
        14,
        75,
        board::DisplayColor::Accent);

    board_.draw_text_region(
        14,
        112,
        212,
        14,
        "M5 POWROT",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void Launcher::render_clock()
{
    clear_shell(board_);
    rendered_clock_screen_text_[0] = '\0';
    rendered_clock_timer_valid_ = false;

    board_.draw_text_region(
        14,
        10,
        212,
        20,
        "ZEGAR",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    render_clock_time_if_changed();

    constexpr const char* kItems[3] = {
        "USTAW CZAS",
        nullptr,
        "POWROT",
    };

    for (std::uint8_t index = 0; index < 3; ++index) {
        if (index == 1U) {
            continue;
        }

        const bool selected = index == clock_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(64 + index * 19);

        board_.draw_text_region(
            22,
            y,
            196,
            18,
            kItems[index],
            2,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 15),
                board::DisplayColor::Accent);
        }
    }

    render_clock_timer_if_changed();

    board_.draw_text_region(
        14,
        122,
        212,
        12,
        "M5 WYBIERZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void Launcher::render_timer_setup()
{
    clear_shell(board_);

    char duration_text[6]{};
    format_mmss(
        countdown_.configured_duration_seconds(),
        duration_text,
        sizeof(duration_text));

    board_.draw_text_region(
        14,
        8,
        212,
        20,
        "MINUTNIK",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        75,
        35,
        90,
        28,
        duration_text,
        3,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        22,
        75,
        196,
        18,
        "M5 START",
        2,
        board::DisplayColor::Accent,
        board::DisplayColor::Background);

    board_.draw_text_region(
        22,
        96,
        196,
        18,
        "BOCZNY +CZAS",
        2,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        14,
        121,
        212,
        12,
        "BOCZNY DLUGO = POWROT",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void Launcher::render_timer_active()
{
    clear_shell(board_);
    rendered_timer_countdown_valid_ = false;

    board_.draw_text_region(
        14,
        6,
        212,
        20,
        "MINUTNIK",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    render_timer_countdown_if_changed();

    const char* primary_action =
        countdown_.state() == countdown::State::Paused
            ? "WZNOW"
            : "PAUZA";
    const char* actions[3] = {
        primary_action,
        "RESETUJ",
        "POWROT",
    };

    for (std::uint8_t index = 0; index < 3; ++index) {
        const bool selected = index == timer_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(61 + index * 19);

        board_.draw_text_region(
            22,
            y,
            196,
            18,
            actions[index],
            2,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 15),
                board::DisplayColor::Accent);
        }
    }

    board_.draw_text_region(
        14,
        121,
        212,
        12,
        "M5 WYBIERZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void Launcher::render_clock_editor(bool editing_hour)
{
    clear_shell(board_);

    char time_text[6]{};
    std::snprintf(
        time_text,
        sizeof(time_text),
        "%02u:%02u",
        static_cast<unsigned>(edit_hour_),
        static_cast<unsigned>(edit_minute_));

    board_.draw_text_region(
        14,
        10,
        212,
        20,
        "USTAW CZAS",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        75,
        42,
        90,
        28,
        time_text,
        3,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(
        82,
        76,
        100,
        16,
        editing_hour ? "GODZINA" : "MINUTA",
        1,
        board::DisplayColor::Accent,
        board::DisplayColor::Background);

    board_.draw_text_region(
        14,
        100,
        212,
        12,
        editing_hour
            ? "BOCZNY +1 | M5 DALEJ"
            : "BOCZNY +1 | M5 ZAPISZ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        14,
        119,
        212,
        12,
        "BOCZNY DLUGO = POWROT",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void Launcher::render_clock_set_failed()
{
    clear_shell(board_);

    board_.draw_text_region(
        14,
        16,
        212,
        20,
        "USTAW CZAS",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        44,
        54,
        170,
        22,
        "NIE ZAPISANO",
        2,
        board::DisplayColor::Danger,
        board::DisplayColor::Background);
    board_.draw_text_region(
        42,
        108,
        180,
        14,
        "M5 / BOCZNY = POWROT",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void Launcher::render_header_time_if_changed()
{
    if (std::strncmp(
            rendered_header_clock_text_,
            cached_clock_text_,
            sizeof(rendered_header_clock_text_)) == 0) {
        return;
    }

    board_.draw_text_region(
        138,
        8,
        36,
        12,
        cached_clock_text_,
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    std::snprintf(
        rendered_header_clock_text_,
        sizeof(rendered_header_clock_text_),
        "%s",
        cached_clock_text_);
}

void Launcher::render_clock_time_if_changed()
{
    if (std::strncmp(
            rendered_clock_screen_text_,
            cached_clock_text_,
            sizeof(rendered_clock_screen_text_)) == 0) {
        return;
    }

    board_.draw_text_region(
        75,
        36,
        90,
        28,
        cached_clock_text_,
        3,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    std::snprintf(
        rendered_clock_screen_text_,
        sizeof(rendered_clock_screen_text_),
        "%s",
        cached_clock_text_);
}

void Launcher::render_clock_timer_if_changed()
{
    const countdown::State state = countdown_.state();
    const std::uint32_t remaining_seconds =
        state == countdown::State::Running
            || state == countdown::State::Paused
        ? countdown_.remaining_seconds()
        : 0U;

    if (rendered_clock_timer_valid_
        && rendered_clock_timer_state_ == state
        && rendered_clock_timer_seconds_ == remaining_seconds) {
        return;
    }

    char label[24] = "MINUTNIK";
    if (state == countdown::State::Running
        || state == countdown::State::Paused) {
        char time_text[6]{};
        format_mmss(remaining_seconds, time_text, sizeof(time_text));
        std::snprintf(
            label,
            sizeof(label),
            "MINUTNIK %s",
            time_text);
    } else if (state == countdown::State::Expired) {
        std::snprintf(label, sizeof(label), "MINUTNIK KONIEC");
    }

    constexpr std::int16_t y = 83;
    const bool selected = clock_selection_ == 1U;
    board_.draw_text_region(
        22,
        y,
        196,
        18,
        label,
        2,
        selected
            ? board::DisplayColor::PrimaryText
            : board::DisplayColor::SecondaryText,
        selected
            ? board::DisplayColor::Surface
            : board::DisplayColor::Background);

    if (selected) {
        board_.draw_line(
            14,
            y,
            14,
            static_cast<std::int16_t>(y + 15),
            board::DisplayColor::Accent);
    }

    rendered_clock_timer_state_ = state;
    rendered_clock_timer_seconds_ = remaining_seconds;
    rendered_clock_timer_valid_ = true;
}

void Launcher::render_timer_countdown_if_changed()
{
    const countdown::State state = countdown_.state();
    const std::uint32_t remaining_seconds =
        state == countdown::State::Running
            || state == countdown::State::Paused
        ? countdown_.remaining_seconds()
        : 0U;

    if (rendered_timer_countdown_valid_
        && rendered_timer_countdown_state_ == state
        && rendered_timer_seconds_ == remaining_seconds) {
        return;
    }

    char time_text[6]{};
    format_mmss(remaining_seconds, time_text, sizeof(time_text));

    board_.draw_text_region(
        75,
        29,
        90,
        28,
        time_text,
        3,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    rendered_timer_countdown_state_ = state;
    rendered_timer_seconds_ = remaining_seconds;
    rendered_timer_countdown_valid_ = true;
}

void Launcher::render_settings()
{
    clear_shell(board_);

    board_.draw_text_region(
        14,
        14,
        212,
        20,
        "USTAWIENIA",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    constexpr const char* kItems[3] = {
        "Dzwiek",
        "Motyw",
        "Powrot",
    };

    for (std::uint8_t index = 0; index < 3; ++index) {
        const bool selected = index == settings_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(40 + index * 23);

        board_.draw_text_region(
            22,
            y,
            196,
            20,
            kItems[index],
            2,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 16),
                board::DisplayColor::Accent);
        }
    }

    board_.draw_text_region(
        14,
        112,
        212,
        14,
        "M5 WYBIERZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void Launcher::render_signal_sound()
{
    clear_shell(board_);

    board_.draw_text_region(
        14,
        12,
        212,
        18,
        "DZWIEK SYGNALU",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    constexpr const char* kItems[4] = {
        "Lagodny",
        "Klasyczny",
        "Pager",
        "Powrot",
    };

    for (std::uint8_t index = 0; index < 4; ++index) {
        const bool selected = index == signal_sound_selection_;
        const bool active =
            index < 3U
            && signal_sound_index(settings_.signal_sound) == index;
        const std::int16_t y =
            static_cast<std::int16_t>(36 + index * 19);

        board_.draw_text_region(
            22,
            y,
            196,
            18,
            kItems[index],
            2,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 15),
                board::DisplayColor::Accent);
        }

        if (active) {
            board_.fill_circle(
                211,
                static_cast<std::int16_t>(y + 7),
                3,
                board::DisplayColor::Accent);
        }
    }

    board_.draw_text_region(
        14,
        116,
        212,
        14,
        "M5 WYBIERZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}
void Launcher::render_theme()
{
    clear_shell(board_);

    board_.draw_text_region(
        14,
        12,
        212,
        18,
        "MOTYW",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    constexpr const char* kItems[6] = {
        "Nikos",
        "Bursztyn",
        "Grafit",
        "Lava",
        "Matrix",
        "Powrot",
    };

    std::uint8_t first_visible = 0;
    if (theme_selection_ >= 4U) {
        first_visible = static_cast<std::uint8_t>(theme_selection_ - 3U);
    }

    for (std::uint8_t slot = 0; slot < 4; ++slot) {
        const std::uint8_t index =
            static_cast<std::uint8_t>(first_visible + slot);
        const bool selected = index == theme_selection_;
        const bool active =
            index < 5U
            && theme_index(settings_.theme) == index;
        const std::int16_t y =
            static_cast<std::int16_t>(36 + slot * 19);

        board_.draw_text_region(
            22,
            y,
            196,
            18,
            kItems[index],
            2,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 15),
                board::DisplayColor::Accent);
        }

        if (active) {
            board_.fill_circle(
                211,
                static_cast<std::int16_t>(y + 7),
                3,
                board::DisplayColor::Accent);
        }
    }

    board_.draw_text_region(
        14,
        116,
        212,
        14,
        "M5 WYBIERZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}


void Launcher::render_enable_communicator()
{
    clear_shell(board_);

    board_.draw_text_region(
        18,
        16,
        204,
        22,
        "WLACZYC",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        18,
        39,
        204,
        22,
        "KOMUNIKATOR?",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    constexpr const char* kChoices[2] = {"TAK", "NIE"};
    for (std::uint8_t index = 0; index < 2; ++index) {
        const bool selected = confirmation_selection_ == index;
        board_.draw_text_region(32, static_cast<std::int16_t>(70 + index * 25),
            176, 22, kChoices[index], 2,
            selected ? board::DisplayColor::PrimaryText : board::DisplayColor::SecondaryText,
            selected ? board::DisplayColor::Surface : board::DisplayColor::Background);
        if (selected) {
            board_.draw_text_region(18, static_cast<std::int16_t>(70 + index * 25),
                12, 22, ">", 2, board::DisplayColor::Accent,
                board::DisplayColor::Background);
        }
    }
    board_.draw_text_region(44, 121, 190, 12,
        "M5 WYBIERZ | BOCZNY DALEJ", 1,
        board::DisplayColor::SecondaryText, board::DisplayColor::Background);
}

void Launcher::render_active_communicator()
{
    clear_shell(board_);

    board_.draw_text_region(
        18,
        10,
        204,
        22,
        "KOMUNIKATOR",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        18,
        32,
        204,
        22,
        "AKTYWNY",
        2,
        board::DisplayColor::StatusActive,
        board::DisplayColor::Background);

    constexpr const char* kChoices[2] = {
        "WEJDZ",
        "WYLACZ",
    };

    for (std::uint8_t index = 0; index < 2; ++index) {
        const bool selected =
            index == active_communicator_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(59 + index * 28);

        board_.draw_text_region(
            22,
            y,
            196,
            22,
            kChoices[index],
            2,
            selected
                ? board::DisplayColor::PrimaryText
                : board::DisplayColor::SecondaryText,
            selected
                ? board::DisplayColor::Surface
                : board::DisplayColor::Background);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 17),
                board::DisplayColor::Accent);
        }
    }

    board_.draw_text_region(
        14,
        116,
        212,
        14,
        "M5 WYBIERZ | BOCZNY DALEJ",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void Launcher::render_disable_communicator()
{
    clear_shell(board_);
    board_.draw_text_region(18, 12, 204, 22, "WYLACZYC", 2,
        board::DisplayColor::PrimaryText, board::DisplayColor::Background);
    board_.draw_text_region(18, 35, 204, 22, "KOMUNIKATOR?", 2,
        board::DisplayColor::PrimaryText, board::DisplayColor::Background);
    constexpr const char* kChoices[2] = {"TAK", "NIE"};
    for (std::uint8_t index = 0; index < 2; ++index) {
        const bool selected = confirmation_selection_ == index;
        board_.draw_text_region(32, static_cast<std::int16_t>(67 + index * 25),
            176, 22, kChoices[index], 2,
            selected ? board::DisplayColor::PrimaryText : board::DisplayColor::SecondaryText,
            selected ? board::DisplayColor::Surface : board::DisplayColor::Background);
        if (selected) {
            board_.draw_text_region(18, static_cast<std::int16_t>(67 + index * 25),
                12, 22, ">", 2, board::DisplayColor::Accent,
                board::DisplayColor::Background);
        }
    }
    board_.draw_text_region(44, 121, 190, 12,
        "M5 WYBIERZ | BOCZNY DALEJ", 1,
        board::DisplayColor::SecondaryText, board::DisplayColor::Background);
}

void Launcher::render_shutdown_confirm()
{
    clear_shell(board_);

    board_.draw_text_region(
        18,
        12,
        200,
        20,
        "WYLACZYC",
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);

    board_.draw_text_region(18, 35, 204, 22, "NIKOS OS?", 2,
        board::DisplayColor::PrimaryText, board::DisplayColor::Background);
    constexpr const char* kChoices[2] = {"TAK", "NIE"};
    for (std::uint8_t index = 0; index < 2; ++index) {
        const bool selected = confirmation_selection_ == index;
        board_.draw_text_region(32, static_cast<std::int16_t>(67 + index * 25),
            176, 22, kChoices[index], 2,
            selected ? (index == 0 ? board::DisplayColor::Danger : board::DisplayColor::PrimaryText)
                     : board::DisplayColor::SecondaryText,
            selected ? board::DisplayColor::Surface : board::DisplayColor::Background);
        if (selected) {
            board_.draw_text_region(18, static_cast<std::int16_t>(67 + index * 25),
                12, 22, ">", 2, board::DisplayColor::Accent,
                board::DisplayColor::Background);
        }
    }
    board_.draw_text_region(44, 121, 190, 12,
        "M5 WYBIERZ | BOCZNY DALEJ", 1,
        board::DisplayColor::SecondaryText, board::DisplayColor::Background);
}

void Launcher::render_battery_if_changed()
{
    if (rendered_battery_valid_
        && rendered_battery_percent_ == cached_battery_percent_) {
        return;
    }

    char battery_text[16]{};
    if (cached_battery_percent_ >= 0) {
        std::snprintf(
            battery_text,
            sizeof(battery_text),
            "BAT %ld%%",
            static_cast<long>(cached_battery_percent_));
    } else {
        std::snprintf(battery_text, sizeof(battery_text), "BAT --%%");
    }

    board_.draw_text_region(
        178,
        8,
        54,
        12,
        battery_text,
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);

    rendered_battery_percent_ = cached_battery_percent_;
    rendered_battery_valid_ = true;
}

}  // namespace nikos::launcher
