#pragma once
#include <cstdint>
#include "board/board.hpp"
#include "font_core/font_core.hpp"
namespace nikos::facts {
class Quiz final {
public:
    explicit Quiz(board::Board& board)
        : board_(board), renderer_(board) {}
    void start();
    void redraw();
    bool update(const board::InputState& input); // true = return to Facts menu
private:
    enum class Screen : std::uint8_t { Question, Feedback, Final };
    void render_question();
    void render_feedback();
    void render_final();
    void draw_wrapped(const char* text, std::int16_t y, std::uint8_t max_lines);
    std::uint16_t text_width(const char* text) const;
    void layout_text_pages(const char* text, std::uint8_t max_lines);
    void draw_text_page(const char* text, std::int16_t y,
        std::int16_t height, std::uint8_t max_lines);
    void draw_selected_text(const char* text, std::int16_t x,
        std::int16_t y, std::int16_t width, std::int16_t height,
        board::DisplayColor foreground, board::DisplayColor background);
    board::Board& board_;
    font_core::Renderer renderer_;
    std::uint8_t text_page_ = 0;
    std::uint8_t text_page_count_ = 1;
    struct Range { std::uint16_t begin; std::uint16_t end; };
    Range text_ranges_[8]{};
    char text_buffer_[192]{};
    std::uint8_t indices_[10]{};
    std::uint8_t position_ = 0;
    std::uint8_t score_ = 0;
    std::uint8_t selected_ = 0;
    std::uint8_t final_selected_ = 0;
    bool last_correct_ = false;
    Screen screen_ = Screen::Question;
};
}
