#pragma once
#include "board/board.hpp"
#include "font_core/font_core.hpp"
#include "coin_flip/coin_rules.hpp"
namespace nikos::coin_flip {
class CoinFlipApp final {
public:
    enum class Result : std::uint8_t { Running, ExitRequested };
    explicit CoinFlipApp(board::Board& board) : board_(board), font_(board) {}
    void begin();
    void end() { active_ = false; state_ = {}; }
    void redraw();
    void suspend() { state_.suspend(); }
    void resume();
    void resume_if_suspended() { if (state_.suspended) resume(); }
    Result update(const board::InputState& input);
private:
    static std::uint32_t now_ms();
    void render();
    void draw_coin(std::uint8_t frame);
    board::Board& board_;
    font_core::Renderer font_;
    State state_{};
    bool active_ = false;
};
}
