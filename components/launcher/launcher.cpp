#include "launcher/launcher.hpp"

#include <array>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {

struct Entry {
    const char* label;
    nikos::launcher::Action action;
};

constexpr std::array<Entry, 4> kEntries = {{
    {"Communicator", nikos::launcher::Action::OpenCommunicator},
    {"RadioLab", nikos::launcher::Action::OpenRadioLab},
    {"Minutnik", nikos::launcher::Action::None},
    {"Rozrywka", nikos::launcher::Action::None},
}};

constexpr std::uint32_t kSignalFrameMs = 110;
constexpr std::uint32_t kSyncFrameMs = 140;
constexpr std::uint32_t kFormFrameMs = 170;
constexpr std::uint32_t kBatterySampleIntervalMs = 1000;

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

Launcher::Launcher(board::Board& board)
    : board_(board)
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

void Launcher::begin()
{
    selected_index_ = 0;
    const std::uint32_t now = now_ms();
    update_battery_sample(now);
    render();
}

Action Launcher::update()
{
    const std::uint32_t now = now_ms();
    update_battery_sample(now);
    render_battery_if_changed();

    const board::InputState input = board_.poll_input();

    if (input.secondary_long) {
        return Action::None;
    }

    if (input.secondary_short) {
        selected_index_ =
            static_cast<std::uint8_t>((selected_index_ + 1U) % kEntries.size());
        render();
        return Action::None;
    }

    if (input.primary_short) {
        return kEntries[selected_index_].action;
    }

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
    clear_shell(board_);
    rendered_battery_valid_ = false;

    board_.draw_text_region(
        10,
        6,
        118,
        18,
        "NIKOS",
        2,
        board::DisplayColor::Ivory,
        board::DisplayColor::Navy);
    board_.draw_text_region(
        117,
        6,
        34,
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

    for (std::size_t index = 0; index < kEntries.size(); ++index) {
        const bool selected = index == selected_index_;
        const std::int16_t row_y =
            static_cast<std::int16_t>(32 + index * 19);

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

        board_.draw_text_region(
            18,
            row_y,
            205,
            18,
            kEntries[index].label,
            2,
            selected
                ? board::DisplayColor::Ivory
                : board::DisplayColor::MutedBlue,
            selected
                ? board::DisplayColor::PanelNavy
                : board::DisplayColor::Navy);
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
