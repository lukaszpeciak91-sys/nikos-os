#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "board/board.hpp"
#include "power/display_lifecycle.hpp"

namespace nikos::snake {

class SnakeApp final {
public:
    enum class UpdateResult : std::uint8_t {
        Running,
        ExitRequested,
    };

    SnakeApp(
        board::Board& board,
        power::DisplayLifecycle& display_lifecycle);

    void begin();
    void end();
    void redraw();
    UpdateResult update(const board::InputState& input);

private:
    static constexpr std::uint8_t kGridWidth = 40;
    static constexpr std::uint8_t kGridHeight = 20;
    static constexpr std::size_t kGridCellCount =
        static_cast<std::size_t>(kGridWidth)
        * static_cast<std::size_t>(kGridHeight);

    enum class State : std::uint8_t {
        Playing,
        GameOver,
        TimeLimit,
    };

    enum class Direction : std::uint8_t {
        Up,
        Right,
        Down,
        Left,
    };

    struct Cell {
        std::uint8_t x = 0;
        std::uint8_t y = 0;
    };

    static std::uint32_t now_ms();

    void start_game(std::uint32_t now);
    void turn_left();
    void turn_right();
    void advance(std::uint32_t now);
    void spawn_food();

    bool cell_occupied(
        const Cell& cell,
        std::size_t count) const;
    bool same_cell(const Cell& lhs, const Cell& rhs) const;
    std::uint32_t move_interval_ms() const;

    void render_game();
    void render_hud();
    void render_game_over();
    void render_time_limit();

    board::Board& board_;
    power::DisplayLifecycle& display_lifecycle_;

    std::array<Cell, kGridCellCount> segments_{};
    std::size_t length_ = 0;
    Cell food_{};
    Direction direction_ = Direction::Right;
    State state_ = State::Playing;

    std::uint16_t score_ = 0;
    std::uint16_t record_ = 0;

    bool active_ = false;
    bool turn_consumed_since_move_ = false;
    std::uint32_t session_started_ms_ = 0;
    std::uint32_t last_move_ms_ = 0;
};

}  // namespace nikos::snake
