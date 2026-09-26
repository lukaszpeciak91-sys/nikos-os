#include "snake/snake_app.hpp"

#include <algorithm>
#include <cstdio>

#include "esp_random.h"
#include "esp_timer.h"

namespace {

constexpr std::int16_t kHudHeightPx = 15;
constexpr std::int16_t kCellSizePx = 6;
constexpr std::uint32_t kInitialMoveIntervalMs = 220;
constexpr std::uint32_t kMinimumMoveIntervalMs = 95;
constexpr std::uint32_t kSpeedStepMs = 8;
constexpr std::uint32_t kSessionLimitMs = 10U * 60U * 1000U;
constexpr std::size_t kInitialSnakeLength = 4;

}  // namespace

namespace nikos::snake {

std::array<SnakeApp::Cell, SnakeApp::kGridCellCount>
    SnakeApp::segments_{};

SnakeApp::SnakeApp(
    board::Board& board,
    power::DisplayLifecycle& display_lifecycle)
    : board_(board),
      display_lifecycle_(display_lifecycle)
{
}

void SnakeApp::begin()
{
    active_ = true;
    session_started_ms_ = now_ms();
    start_game(session_started_ms_);
    display_lifecycle_.note_visible_activity();
    render_game();
}

void SnakeApp::end()
{
    active_ = false;
}

void SnakeApp::redraw()
{
    if (!active_) {
        return;
    }

    switch (state_) {
        case State::Playing:
            render_game();
            break;
        case State::GameOver:
            render_game_over();
            break;
        case State::TimeLimit:
            render_time_limit();
            break;
    }
}

SnakeApp::UpdateResult SnakeApp::update(
    const board::InputState& input)
{
    if (!active_) {
        return UpdateResult::ExitRequested;
    }

    if (input.secondary_long) {
        return UpdateResult::ExitRequested;
    }

    if (state_ == State::TimeLimit) {
        if (input.primary_short || input.secondary_short) {
            return UpdateResult::ExitRequested;
        }
        return UpdateResult::Running;
    }

    const std::uint32_t now = now_ms();

    if (now - session_started_ms_ >= kSessionLimitMs) {
        state_ = State::TimeLimit;
        render_time_limit();
        return UpdateResult::Running;
    }

    if (state_ == State::GameOver) {
        if (input.primary_short) {
            start_game(now);
            display_lifecycle_.note_visible_activity();
            render_game();
        } else if (input.secondary_short) {
            return UpdateResult::ExitRequested;
        }
        return UpdateResult::Running;
    }

    if (!turn_consumed_since_move_) {
        if (input.primary_short) {
            turn_left();
            turn_consumed_since_move_ = true;
        } else if (input.secondary_short) {
            turn_right();
            turn_consumed_since_move_ = true;
        }
    }

    if (now - last_move_ms_ >= move_interval_ms()) {
        advance(now);
    }

    return UpdateResult::Running;
}

std::uint32_t SnakeApp::now_ms()
{
    return static_cast<std::uint32_t>(
        esp_timer_get_time() / 1000ULL);
}

void SnakeApp::start_game(std::uint32_t now)
{
    state_ = State::Playing;
    direction_ = Direction::Right;
    score_ = 0;
    length_ = kInitialSnakeLength;
    turn_consumed_since_move_ = false;
    last_move_ms_ = now;

    const std::uint8_t center_y = kGridHeight / 2U;
    const std::uint8_t head_x = kGridWidth / 2U;

    for (std::size_t i = 0; i < length_; ++i) {
        segments_[i] = Cell{
            static_cast<std::uint8_t>(head_x - i),
            center_y,
        };
    }

    spawn_food();
}

void SnakeApp::turn_left()
{
    switch (direction_) {
        case Direction::Up:
            direction_ = Direction::Left;
            break;
        case Direction::Left:
            direction_ = Direction::Down;
            break;
        case Direction::Down:
            direction_ = Direction::Right;
            break;
        case Direction::Right:
            direction_ = Direction::Up;
            break;
    }
}

void SnakeApp::turn_right()
{
    switch (direction_) {
        case Direction::Up:
            direction_ = Direction::Right;
            break;
        case Direction::Right:
            direction_ = Direction::Down;
            break;
        case Direction::Down:
            direction_ = Direction::Left;
            break;
        case Direction::Left:
            direction_ = Direction::Up;
            break;
    }
}

void SnakeApp::advance(std::uint32_t now)
{
    Cell next = segments_[0];

    switch (direction_) {
        case Direction::Up:
            next.y = next.y == 0
                ? static_cast<std::uint8_t>(kGridHeight - 1U)
                : static_cast<std::uint8_t>(next.y - 1U);
            break;
        case Direction::Right:
            next.x = static_cast<std::uint8_t>(
                (next.x + 1U) % kGridWidth);
            break;
        case Direction::Down:
            next.y = static_cast<std::uint8_t>(
                (next.y + 1U) % kGridHeight);
            break;
        case Direction::Left:
            next.x = next.x == 0
                ? static_cast<std::uint8_t>(kGridWidth - 1U)
                : static_cast<std::uint8_t>(next.x - 1U);
            break;
    }

    const bool eating = same_cell(next, food_);
    const std::size_t collision_count =
        eating ? length_ : (length_ > 0 ? length_ - 1U : 0U);

    if (cell_occupied(next, collision_count)) {
        state_ = State::GameOver;
        record_ = std::max(record_, score_);
        render_game_over();
        return;
    }

    if (eating && length_ < segments_.size()) {
        ++length_;
    }

    for (std::size_t i = length_ - 1U; i > 0; --i) {
        segments_[i] = segments_[i - 1U];
    }
    segments_[0] = next;

    if (eating) {
        ++score_;
        record_ = std::max(record_, score_);

        if (length_ >= segments_.size()) {
            state_ = State::GameOver;
            render_game_over();
            return;
        }

        spawn_food();
    }

    last_move_ms_ = now;
    turn_consumed_since_move_ = false;

    // Active gameplay is meaningful visible activity. Refreshing only on a
    // movement tick keeps the global display lifecycle awake without tying
    // power policy to the 20 ms main-loop cadence.
    display_lifecycle_.note_visible_activity();
    render_game();
}

void SnakeApp::spawn_food()
{
    const std::size_t start =
        static_cast<std::size_t>(esp_random() % kGridCellCount);

    for (std::size_t offset = 0; offset < kGridCellCount; ++offset) {
        const std::size_t index =
            (start + offset) % kGridCellCount;
        const Cell candidate{
            static_cast<std::uint8_t>(index % kGridWidth),
            static_cast<std::uint8_t>(index / kGridWidth),
        };

        if (!cell_occupied(candidate, length_)) {
            food_ = candidate;
            return;
        }
    }
}

bool SnakeApp::cell_occupied(
    const Cell& cell,
    std::size_t count) const
{
    const std::size_t bounded_count =
        std::min(count, length_);
    for (std::size_t i = 0; i < bounded_count; ++i) {
        if (same_cell(cell, segments_[i])) {
            return true;
        }
    }
    return false;
}

bool SnakeApp::same_cell(
    const Cell& lhs,
    const Cell& rhs) const
{
    return lhs.x == rhs.x && lhs.y == rhs.y;
}

std::uint32_t SnakeApp::move_interval_ms() const
{
    const std::uint32_t reduction =
        static_cast<std::uint32_t>(score_) * kSpeedStepMs;

    if (reduction >= kInitialMoveIntervalMs - kMinimumMoveIntervalMs) {
        return kMinimumMoveIntervalMs;
    }

    return kInitialMoveIntervalMs - reduction;
}

void SnakeApp::render_game()
{
    render_hud();

    board_.fill_rect(
        0,
        kHudHeightPx,
        240,
        120,
        board::DisplayColor::Background);

    board_.fill_rect(
        static_cast<std::int16_t>(food_.x * kCellSizePx),
        static_cast<std::int16_t>(
            kHudHeightPx + food_.y * kCellSizePx),
        kCellSizePx,
        kCellSizePx,
        board::DisplayColor::Attention);

    for (std::size_t i = length_; i > 0; --i) {
        const std::size_t index = i - 1U;
        const Cell& segment = segments_[index];
        board_.fill_rect(
            static_cast<std::int16_t>(segment.x * kCellSizePx),
            static_cast<std::int16_t>(
                kHudHeightPx + segment.y * kCellSizePx),
            kCellSizePx,
            kCellSizePx,
            index == 0
                ? board::DisplayColor::Accent
                : board::DisplayColor::PrimaryText);
    }
}

void SnakeApp::render_hud()
{
    char score_text[20]{};
    char record_text[20]{};

    std::snprintf(
        score_text,
        sizeof(score_text),
        "WYNIK %u",
        static_cast<unsigned>(score_));
    std::snprintf(
        record_text,
        sizeof(record_text),
        "REKORD %u",
        static_cast<unsigned>(record_));

    board_.draw_text_region(
        4,
        2,
        112,
        11,
        score_text,
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        138,
        2,
        98,
        11,
        record_text,
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void SnakeApp::render_game_over()
{
    board_.clear_screen();

    char score_text[24]{};
    char record_text[24]{};
    std::snprintf(
        score_text,
        sizeof(score_text),
        "WYNIK: %u",
        static_cast<unsigned>(score_));
    std::snprintf(
        record_text,
        sizeof(record_text),
        "REKORD: %u",
        static_cast<unsigned>(record_));

    board_.draw_text_region(
        54,
        16,
        150,
        24,
        "KONIEC GRY",
        2,
        board::DisplayColor::Attention,
        board::DisplayColor::Background);
    board_.draw_text_region(
        70,
        48,
        120,
        18,
        score_text,
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        70,
        70,
        120,
        18,
        record_text,
        2,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        18,
        104,
        210,
        14,
        "M5 = JESZCZE RAZ",
        1,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        18,
        120,
        210,
        14,
        "BOCZNY = POWROT",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

void SnakeApp::render_time_limit()
{
    board_.clear_screen();

    char score_text[24]{};
    std::snprintf(
        score_text,
        sizeof(score_text),
        "WYNIK: %u",
        static_cast<unsigned>(score_));

    board_.draw_text_region(
        34,
        25,
        190,
        20,
        "CZAS NA PRZERWE",
        2,
        board::DisplayColor::Attention,
        board::DisplayColor::Background);
    board_.draw_text_region(
        70,
        59,
        120,
        20,
        score_text,
        2,
        board::DisplayColor::PrimaryText,
        board::DisplayColor::Background);
    board_.draw_text_region(
        31,
        105,
        195,
        14,
        "M5 / BOCZNY = POWROT",
        1,
        board::DisplayColor::SecondaryText,
        board::DisplayColor::Background);
}

}  // namespace nikos::snake
