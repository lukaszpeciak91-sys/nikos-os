#pragma once
#include <cstdint>
#include "board/board.hpp"
#include "font_core/font_core.hpp"

namespace nikos::vw_garage {
class VwGarageApp final {
public:
    enum class UpdateResult : std::uint8_t { Running, ExitRequested };
    explicit VwGarageApp(board::Board& board);
    void begin();
    void end();
    void redraw();
    UpdateResult update(const board::InputState& input);
private:
    enum class View : std::uint8_t { List, Card };
    static constexpr std::uint8_t kVisibleRows = 4;
    void ensure_selection_visible();
    void return_to_list();
    void draw_header();
    void render_list();
    void render_card();

    board::Board& board_;
    font_core::Renderer renderer_;
    std::uint8_t selected_index_ = 0;
    std::uint8_t viewport_start_ = 0;
    View view_ = View::List;
    bool active_ = false;
};
}  // namespace nikos::vw_garage
