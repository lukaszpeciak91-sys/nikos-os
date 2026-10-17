#pragma once
#include <cstdint>
#include "board/board.hpp"
#include "power/display_lifecycle.hpp"
#include "space_defender/rules.hpp"
namespace nikos::space_defender {
class SpaceDefenderApp final {
public:
    enum class Result : std::uint8_t { Running, ExitRequested };
    SpaceDefenderApp(board::Board& board, power::DisplayLifecycle& display)
        : board_(board), display_(display) {}
    void begin();
    void end() { active_ = false; }
    void suspend() { if (active_) suspended_ = true; }
    void resume();
    void resume_if_suspended() { if (suspended_) resume(); }
    void redraw();
    Result update(const board::InputState& input);
private:
    static std::uint32_t now_ms();
    void new_round(std::uint32_t now);
    void render(bool full = false);
    void render_game(bool full);
    void render_menu();
    void draw_ship(std::uint8_t lane, bool visible);
    void draw_enemy(const rules::Enemy& e);
    board::Board& board_;
    power::DisplayLifecycle& display_;
    rules::World world_{};
    bool active_ = false, suspended_ = false, frame_valid_ = false;
    std::uint8_t menu_selection_ = 0;
    std::uint32_t session_started_ms_ = 0, last_tick_ms_ = 0, last_frame_ms_ = 0;
    std::uint32_t saved_record_ = 0;
};
}
