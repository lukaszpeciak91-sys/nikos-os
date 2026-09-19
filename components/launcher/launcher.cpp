#include "launcher/launcher.hpp"

#include <array>
#include <cstdio>

namespace {

constexpr std::array<const char*, 3> kEntries = {
    "RadioLab",
    "Minutnik",
    "Rozrywka",
};

}  // namespace

namespace nikos::launcher {

Launcher::Launcher(board::Board& board)
    : board_(board)
{
}

void Launcher::show_splash()
{
    board_.clear_screen();
    board_.draw_text_region(42, 48, 160, 32, "Nikoś OS", 3);
}

void Launcher::begin()
{
    selected_index_ = 0;
    render();
}

Action Launcher::update()
{
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

    if (input.primary_short && selected_index_ == 0) {
        return Action::OpenRadioLab;
    }

    return Action::None;
}

void Launcher::render()
{
    board_.clear_screen();
    board_.draw_text_region(4, 3, 232, 20, "Nikoś OS", 2);

    for (std::size_t index = 0; index < kEntries.size(); ++index) {
        char line[24]{};
        std::snprintf(
            line,
            sizeof(line),
            "%c %s",
            index == selected_index_ ? '>' : ' ',
            kEntries[index]);

        board_.draw_text_region(
            12,
            static_cast<std::int16_t>(30 + index * 24),
            216,
            20,
            line,
            2,
            index == selected_index_
                ? board::DisplayColor::Green
                : board::DisplayColor::White);
    }

    board_.draw_text_region(8, 112, 224, 14, "M5 OPEN   SIDE NEXT", 1);
}

}  // namespace nikos::launcher
