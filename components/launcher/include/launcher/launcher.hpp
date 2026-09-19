#pragma once

#include <cstdint>

#include "board/board.hpp"

namespace nikos::launcher {

enum class Action : std::uint8_t {
    None,
    OpenRadioLab,
};

class Launcher final {
public:
    explicit Launcher(board::Board& board);

    void show_splash();
    void begin();
    Action update();

private:
    void render();

    board::Board& board_;
    std::uint8_t selected_index_ = 0;
};

}  // namespace nikos::launcher
