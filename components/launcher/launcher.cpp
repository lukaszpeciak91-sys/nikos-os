#include "launcher/launcher.hpp"

#include <array>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

struct Entry {
    const char* label;
};

constexpr std::array<Entry, 6> kEntries = {{
    {"Komunikator"},
    {"Narzędzia"},
    {"Rozrywka"},
    {"Zegar"},
    {"Ustawienia"},
    {"Wyłącz"},
}};

constexpr std::size_t kVisibleLauncherRows = 4;

constexpr std::uint32_t kSignalFrameMs = 110;
constexpr std::uint32_t kSyncFrameMs = 140;
constexpr std::uint32_t kFormFrameMs = 170;
constexpr std::uint32_t kBatterySampleIntervalMs = 1000;

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
        nikos::board::DisplayColor::Ivory,
        nikos::board::DisplayColor::Navy);
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
        nikos::board::DisplayColor::Ivory,
        nikos::board::DisplayColor::Navy);
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
        nikos::board::DisplayColor::Ivory,
        nikos::board::DisplayColor::Navy);

    for (std::int16_t y : {58, 64, 70}) {
        board.draw_line(
            39,
            y,
            184,
            y,
            nikos::board::DisplayColor::Navy);
        board.draw_line(
            39,
            static_cast<std::int16_t>(y + 1),
            184,
            static_cast<std::int16_t>(y + 1),
            nikos::board::DisplayColor::Navy);
    }

    if (show_os) {
        board.draw_text_region(
            173,
            78,
            52,
            24,
            "OS",
            2,
            nikos::board::DisplayColor::AccentGreen,
            nikos::board::DisplayColor::Navy);
    }
}

}  // namespace

namespace nikos::launcher {

Launcher::Launcher(
    board::Board& board,
    settings::State& settings,
    signal_sound::Player& signal_sound)
    : board_(board),
      settings_(settings),
      signal_sound_(signal_sound)
{
}

void Launcher::show_splash()
{
    clear_shell(board_);

    // SIGNAL: sparse fragments arrive first.
    draw_fragment(board_, 30, 61, 18, board::DisplayColor::Ivory);
    draw_fragment(board_, 58, 48, 12, board::DisplayColor::MutedBlue);
    draw_fragment(board_, 78, 70, 19, board::DisplayColor::Ivory);
    draw_fragment(board_, 108, 55, 14, board::DisplayColor::MutedBlue);
    draw_fragment(board_, 137, 67, 22, board::DisplayColor::Ivory);
    draw_fragment(board_, 171, 51, 16, board::DisplayColor::MutedBlue);
    draw_fragment(board_, 194, 73, 18, board::DisplayColor::Ivory);
    vTaskDelay(pdMS_TO_TICKS(kSignalFrameMs));

    // SYNCHRONIZING: fragments settle onto shared scan bands.
    clear_logo_area(board_);
    for (std::int16_t x : {40, 70, 100, 130, 160}) {
        draw_fragment(board_, x, 51, 20, board::DisplayColor::MutedBlue);
        draw_fragment(board_, x, 61, 24, board::DisplayColor::Ivory);
        draw_fragment(board_, x, 71, 18, board::DisplayColor::MutedBlue);
    }
    draw_fragment(board_, 55, 81, 13, board::DisplayColor::Ivory);
    draw_fragment(board_, 151, 81, 16, board::DisplayColor::Ivory);
    vTaskDelay(pdMS_TO_TICKS(kSyncFrameMs));

    // FORMING: the wordmark resolves before the secondary OS mark appears.
    draw_sliced_logo(board_, false);
    vTaskDelay(pdMS_TO_TICKS(kFormFrameMs));

    // READY: final clean branded mark. app_main keeps this frame visible ~750 ms.
    draw_sliced_logo(board_, true);
}

void Launcher::begin(bool communicator_active)
{
    screen_ = Screen::Main;
    communicator_active_ = communicator_active;
    selected_index_ = 0;
    tools_selection_ = 0;
    settings_selection_ = 0;
    signal_sound_selection_ = signal_sound_index(settings_.signal_sound);
    active_communicator_selection_ = 0;

    const std::uint32_t now = now_ms();
    update_battery_sample(now);
    render();
}

void Launcher::begin_tools(bool communicator_active)
{
    screen_ = Screen::Tools;
    communicator_active_ = communicator_active;
    selected_index_ = 1;
    tools_selection_ = 0;

    const std::uint32_t now = now_ms();
    update_battery_sample(now);
    render();
}

Action Launcher::update()
{
    const std::uint32_t now = now_ms();
    update_battery_sample(now);

    if (screen_ == Screen::Main) {
        render_battery_if_changed();
    }

    const board::InputState input = board_.poll_input();

    if (screen_ == Screen::EnableCommunicator) {
        if (input.primary_short) {
            return Action::StartCommunicator;
        }

        if (input.secondary_short) {
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
            return active_communicator_selection_ == 0
                ? Action::OpenCommunicator
                : Action::StopCommunicator;
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

    if (screen_ == Screen::Entertainment
        || screen_ == Screen::Clock) {
        if (input.secondary_long || input.primary_short) {
            screen_ = Screen::Main;
            render();
            return Action::None;
        }

        if (input.secondary_short) {
            // There is exactly one selectable item: visible "Powrót".
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
                static_cast<std::uint8_t>((settings_selection_ + 1U) % 2U);
            render();
            return Action::None;
        }

        if (input.primary_short) {
            if (settings_selection_ == 0) {
                signal_sound_selection_ =
                    signal_sound_index(settings_.signal_sound);
                screen_ = Screen::SignalSound;
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

    if (screen_ == Screen::ShutdownConfirm) {
        if (input.primary_short) {
            return Action::ShutdownRequested;
        }

        if (input.secondary_short || input.secondary_long) {
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
            screen_ = communicator_active_
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
            screen_ = Screen::Clock;
            break;
        case 4:
            settings_selection_ = 0;
            screen_ = Screen::Settings;
            break;
        case 5:
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
        case Screen::Settings:
            render_settings();
            break;
        case Screen::SignalSound:
            render_signal_sound();
            break;
        case Screen::EnableCommunicator:
            render_enable_communicator();
            break;
        case Screen::ActiveCommunicator:
            render_active_communicator();
            break;
        case Screen::ShutdownConfirm:
            render_shutdown_confirm();
            break;
    }
}

void Launcher::render_main()
{
    clear_shell(board_);
    rendered_battery_valid_ = false;

    board_.draw_polish_ui_text_region(
        10,
        6,
        60,
        18,
        "NIKOŚ",
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);
    board_.draw_polish_ui_text_region(
        72,
        6,
        30,
        18,
        "OS",
        2,
        board::DisplayColor::AccentGreen,
        board::DisplayColor::Navy);
    board_.draw_line(
        10,
        26,
        229,
        26,
        board::DisplayColor::MutedBlue);

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
                ? board::DisplayColor::PanelNavy
                : board::DisplayColor::Navy;

        if (selected) {
            board_.draw_text_region(
                8,
                static_cast<std::int16_t>(row_y - 2),
                224,
                18,
                "",
                1,
                board::DisplayColor::Ivory,
                board::DisplayColor::PanelNavy);
            board_.draw_line(
                8,
                static_cast<std::int16_t>(row_y - 1),
                8,
                static_cast<std::int16_t>(row_y + 14),
                board::DisplayColor::AccentGreen);
            board_.draw_line(
                9,
                static_cast<std::int16_t>(row_y - 1),
                9,
                static_cast<std::int16_t>(row_y + 14),
                board::DisplayColor::AccentGreen);
        }

        board_.draw_polish_ui_text_region(
            18,
            row_y,
            184,
            18,
            kEntries[index].label,
            2,
            selected
                ? board::DisplayColor::Ivory
                : board::DisplayColor::MutedBlue,
            row_background);

        if (index == 0) {
            const board::DisplayColor indicator_color =
                communicator_active_
                    ? board::DisplayColor::AccentGreen
                    : board::DisplayColor::MutedBlue;

            board_.fill_circle(
                219,
                static_cast<std::int16_t>(row_y + 7),
                5,
                indicator_color);

            if (!communicator_active_) {
                board_.fill_circle(
                    219,
                    static_cast<std::int16_t>(row_y + 7),
                    2,
                    row_background);
            }
        }
    }

    board_.draw_text_region(
        10,
        112,
        220,
        12,
        "M5 OPEN  |  SIDE NEXT",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void Launcher::render_tools()
{
    clear_shell(board_);

    board_.draw_polish_ui_text_region(
        14,
        14,
        212,
        20,
        "NARZĘDZIA",
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);

    constexpr const char* kTools[2] = {
        "RadioLab",
        "Powrót",
    };

    for (std::uint8_t index = 0; index < 2; ++index) {
        const bool selected = index == tools_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(48 + index * 28);

        board_.draw_polish_ui_text_region(
            22,
            y,
            196,
            22,
            kTools[index],
            2,
            selected
                ? board::DisplayColor::Ivory
                : board::DisplayColor::MutedBlue,
            selected
                ? board::DisplayColor::PanelNavy
                : board::DisplayColor::Navy);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 17),
                board::DisplayColor::AccentGreen);
        }
    }

    board_.draw_polish_ui_text_region(
        14,
        112,
        212,
        14,
        "M5 WYBIERZ  |  SIDE DALEJ",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void Launcher::render_entertainment()
{
    clear_shell(board_);

    board_.draw_polish_ui_text_region(
        14,
        18,
        212,
        20,
        "ROZRYWKA",
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);

    board_.draw_polish_ui_text_region(
        22,
        58,
        196,
        22,
        "Powrót",
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::PanelNavy);
    board_.draw_line(
        14,
        58,
        14,
        75,
        board::DisplayColor::AccentGreen);

    board_.draw_polish_ui_text_region(
        14,
        112,
        212,
        14,
        "M5 POWRÓT",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void Launcher::render_clock()
{
    clear_shell(board_);

    board_.draw_polish_ui_text_region(
        14,
        18,
        212,
        20,
        "ZEGAR",
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);

    board_.draw_polish_ui_text_region(
        22,
        58,
        196,
        22,
        "Powrót",
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::PanelNavy);
    board_.draw_line(
        14,
        58,
        14,
        75,
        board::DisplayColor::AccentGreen);

    board_.draw_polish_ui_text_region(
        14,
        112,
        212,
        14,
        "M5 POWRÓT",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void Launcher::render_settings()
{
    clear_shell(board_);

    board_.draw_polish_ui_text_region(
        14,
        18,
        212,
        20,
        "USTAWIENIA",
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);

    constexpr const char* kItems[2] = {
        "Dźwięk",
        "Powrót",
    };

    for (std::uint8_t index = 0; index < 2; ++index) {
        const bool selected = index == settings_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(50 + index * 28);

        board_.draw_polish_ui_text_region(
            22,
            y,
            196,
            22,
            kItems[index],
            2,
            selected
                ? board::DisplayColor::Ivory
                : board::DisplayColor::MutedBlue,
            selected
                ? board::DisplayColor::PanelNavy
                : board::DisplayColor::Navy);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 17),
                board::DisplayColor::AccentGreen);
        }
    }

    board_.draw_polish_ui_text_region(
        14,
        112,
        212,
        14,
        "M5 WYBIERZ  |  SIDE DALEJ",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void Launcher::render_signal_sound()
{
    clear_shell(board_);

    board_.draw_polish_ui_text_region(
        14,
        12,
        212,
        18,
        "DŹWIĘK SYGNAŁU",
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);

    constexpr const char* kItems[4] = {
        "Łagodny",
        "Klasyczny",
        "Pager",
        "Powrót",
    };

    for (std::uint8_t index = 0; index < 4; ++index) {
        const bool selected = index == signal_sound_selection_;
        const bool active =
            index < 3U
            && signal_sound_index(settings_.signal_sound) == index;
        const std::int16_t y =
            static_cast<std::int16_t>(36 + index * 19);

        board_.draw_polish_ui_text_region(
            22,
            y,
            196,
            18,
            kItems[index],
            2,
            selected
                ? board::DisplayColor::Ivory
                : board::DisplayColor::MutedBlue,
            selected
                ? board::DisplayColor::PanelNavy
                : board::DisplayColor::Navy);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 15),
                board::DisplayColor::AccentGreen);
        }

        if (active) {
            board_.fill_circle(
                211,
                static_cast<std::int16_t>(y + 7),
                3,
                board::DisplayColor::AccentGreen);
        }
    }

    board_.draw_polish_ui_text_region(
        14,
        116,
        212,
        14,
        "M5 WYBIERZ  |  SIDE DALEJ",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void Launcher::render_enable_communicator()
{
    clear_shell(board_);

    board_.draw_polish_ui_text_region(
        14,
        26,
        212,
        22,
        "WŁĄCZYĆ KOMUNIKATOR?",
        1,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);

    board_.draw_polish_ui_text_region(
        18,
        62,
        204,
        18,
        "M5 / PRIMARY: TAK",
        1,
        board::DisplayColor::AccentGreen,
        board::DisplayColor::Navy);

    board_.draw_polish_ui_text_region(
        18,
        86,
        204,
        18,
        "SIDE / SECONDARY: NIE",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void Launcher::render_active_communicator()
{
    clear_shell(board_);

    board_.draw_polish_ui_text_region(
        20,
        18,
        200,
        18,
        "KOMUNIKATOR AKTYWNY",
        1,
        board::DisplayColor::AccentGreen,
        board::DisplayColor::Navy);

    constexpr const char* kChoices[2] = {
        "WEJDŹ",
        "WYŁĄCZ",
    };

    for (std::uint8_t index = 0; index < 2; ++index) {
        const bool selected =
            index == active_communicator_selection_;
        const std::int16_t y =
            static_cast<std::int16_t>(49 + index * 28);

        board_.draw_polish_ui_text_region(
            22,
            y,
            196,
            22,
            kChoices[index],
            2,
            selected
                ? board::DisplayColor::Ivory
                : board::DisplayColor::MutedBlue,
            selected
                ? board::DisplayColor::PanelNavy
                : board::DisplayColor::Navy);

        if (selected) {
            board_.draw_line(
                14,
                y,
                14,
                static_cast<std::int16_t>(y + 17),
                board::DisplayColor::AccentGreen);
        }
    }

    board_.draw_polish_ui_text_region(
        14,
        112,
        212,
        14,
        "M5 WYBIERZ  |  SIDE DALEJ",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
}

void Launcher::render_shutdown_confirm()
{
    clear_shell(board_);

    board_.draw_polish_ui_text_region(
        20,
        20,
        200,
        20,
        "WYŁĄCZYĆ NIKOŚ OS?",
        1,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);

    board_.draw_polish_ui_text_region(
        18,
        58,
        204,
        18,
        "M5 / PRIMARY: TAK",
        1,
        board::DisplayColor::AccentGreen,
        board::DisplayColor::Navy);

    board_.draw_polish_ui_text_region(
        18,
        84,
        204,
        18,
        "SIDE / SECONDARY: NIE",
        1,
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);
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
        board::DisplayColor::MutedBlue,
        board::DisplayColor::Navy);

    rendered_battery_percent_ = cached_battery_percent_;
    rendered_battery_valid_ = true;
}

}  // namespace nikos::launcher
