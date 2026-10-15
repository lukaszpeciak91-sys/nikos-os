#pragma once
#include <cstdint>
#include <array>
#include "board/board.hpp"
#include "mini_racer/traffic_rules.hpp"
#include "power/display_lifecycle.hpp"
namespace nikos::mini_racer {
class MiniRacerApp final {
public:
    enum class UpdateResult : std::uint8_t { Running, ExitRequested };
    MiniRacerApp(board::Board& board, power::DisplayLifecycle& display)
        : board_(board), display_(display) {}
    void begin();
    void end() { active_ = false; }
    void redraw();
    void suspend(); // Foreground interruption: preserve exact game state.
    void resume();
    void resume_if_suspended() { if (suspended_) resume(); }
    UpdateResult update(const board::InputState& input);
private:
    enum class State : std::uint8_t { Playing, Paused, GameOver, TimeLimit };
    static std::uint32_t now_ms();
    static constexpr std::uint32_t kSessionLimitMs = 600000;
    static constexpr std::int32_t kSpawnX = 255;
    static constexpr std::int32_t kPlayerLeft = 33;
    static constexpr std::int32_t kPlayerRight = 59;
    static constexpr std::int32_t kEnemyWidth = 26;
    static constexpr std::int32_t kRoadLeft = 5;
    static constexpr std::int32_t kRoadRight = 235;
    void start_round(std::uint32_t now);
    void tick(std::uint32_t now);
    void render();
    void render_playing(bool full = true);
    void render_menu(bool game_over);
    void render_car(std::int16_t x, std::uint8_t lane, board::DisplayColor color);
    board::Board& board_;
    power::DisplayLifecycle& display_;
    State state_ = State::Playing;
    bool active_ = false;
    bool suspended_ = false;
    std::uint8_t player_lane_ = 1;
    rules::Traffic enemies_{};
    rules::Traffic drawn_enemies_{};
    std::uint8_t drawn_player_lane_ = 1;
    bool frame_valid_ = false;
    std::uint32_t last_drawn_score_ = 0;
    std::uint32_t last_tick_ms_ = 0;
    std::uint32_t last_frame_ms_ = 0;
    std::uint32_t session_started_ms_ = 0;
    std::uint32_t active_play_ms_ = 0;
    std::uint32_t score_ = 0;
    std::uint32_t record_ = 0;
    std::uint8_t menu_selection_ = 0;
    std::uint16_t divider_offset_ = 0;
};
}
