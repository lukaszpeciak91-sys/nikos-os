#pragma once
#include <cstdint>
#include "board/board.hpp"
#include "font_core/font_core.hpp"
namespace nikos::font_lab {
class FontLabApp final {
public:
    explicit FontLabApp(board::Board& board) : board_(board), renderer_(board) {}
    void begin();
    void end() { active_ = false; }
    void redraw();
    bool update(const board::InputState& input); // true: exit to Tools
private:
    enum class View : std::uint8_t { List, Sample };
    void render_list();
    void render_sample();
    board::Board& board_;
    font_core::Renderer renderer_;
    bool active_ = false;
    View view_ = View::List;
    std::uint8_t choice_ = 0;
    std::uint8_t page_ = 0;
};
}
