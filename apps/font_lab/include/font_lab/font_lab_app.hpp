#pragma once
#include <cstdint>
#include "board/board.hpp"
namespace nikos::font_lab {
class FontLabApp final {
public:
    explicit FontLabApp(board::Board& board) : board_(board) {}
    void begin();
    void end() { active_ = false; }
    void redraw();
    bool update(const board::InputState& input); // true: exit to Tools
private:
    enum class View : std::uint8_t { List, Sample };
    void render_list();
    void render_sample();
    void render_direct(const char* utf8);
    void render_composed(const char* utf8);
    void render_raster();
    board::Board& board_;
    bool active_ = false;
    View view_ = View::List;
    std::uint8_t choice_ = 0;
    std::uint8_t page_ = 0;
};
}
