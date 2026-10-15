#pragma once
#include <cstdint>
#include "board/board.hpp"
namespace nikos::facts {
class Quiz final {
public:
    explicit Quiz(board::Board& board) : board_(board) {}
    void start();
    void redraw();
    bool update(const board::InputState& input); // true = return to Facts menu
private:
    enum class Screen : std::uint8_t { Question, Feedback, Final };
    void render_question();
    void render_feedback();
    void render_final();
    void draw_wrapped(const char* text, std::int16_t y, std::uint8_t max_lines);
    board::Board& board_;
    std::uint8_t indices_[10]{};
    std::uint8_t position_ = 0;
    std::uint8_t score_ = 0;
    std::uint8_t selected_ = 0;
    std::uint8_t final_selected_ = 0;
    bool last_correct_ = false;
    Screen screen_ = Screen::Question;
};
}
