#include "mini_racer/mini_racer_app.hpp"
#include <algorithm>
#include <cstdio>
#include "esp_random.h"
#include "esp_timer.h"
namespace nikos::mini_racer {
namespace {
constexpr std::uint32_t kFramePeriodMs = 75;
constexpr std::uint32_t kMaxStepMs = 60;
constexpr std::int16_t kLaneCenters[2] = {49, 90};
}
std::uint32_t MiniRacerApp::now_ms() {
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000ULL);
}
void MiniRacerApp::begin() {
    active_ = true;
    suspended_ = false;
    session_started_ms_ = now_ms();
    start_round(session_started_ms_);
}
void MiniRacerApp::start_round(std::uint32_t now) {
    state_ = State::Playing;
    player_lane_ = 1;
    rng_ = rules::Random(esp_random());
    rules::initialize(enemies_, kSpawnX, rng_);
    motion_ = rules::Motion{};
    frame_valid_ = false;
    last_drawn_score_ = 0;
    active_play_ms_ = 0;
    score_ = 0;
    menu_selection_ = 0;
    divider_offset_q8_ = 0;
    last_tick_ms_ = now;
    last_frame_ms_ = now;
    display_.note_visible_activity();
    render();
}
void MiniRacerApp::suspend() {
    if (active_) suspended_ = true;
}
void MiniRacerApp::resume() {
    if (!active_) return;
    suspended_ = false;
    last_tick_ms_ = now_ms();
    last_frame_ms_ = last_tick_ms_;
    redraw();
}
void MiniRacerApp::redraw() {
    if (active_) render();
}
MiniRacerApp::UpdateResult MiniRacerApp::update(const board::InputState& input) {
    if (!active_ || input.secondary_long) return UpdateResult::ExitRequested;
    if (suspended_) return UpdateResult::Running;
    const std::uint32_t now = now_ms();
    // The 10-minute bound is wall-clock time including pause, screen-off
    // and background foreground-takeover intervals.
    if (state_ != State::TimeLimit
        && now - session_started_ms_ >= kSessionLimitMs) {
        state_ = State::TimeLimit;
        menu_selection_ = 1;
        render();
    }
    if (state_ == State::Playing) {
        if (input.secondary_short) {
            state_ = rules::pause(state_);
            menu_selection_ = 0;
            render();
        } else {
            if (input.primary_short) {
                player_lane_ ^= 1U;
            }
            tick(now);
        }
    } else if (state_ == State::Paused || state_ == State::GameOver) {
        if (input.secondary_short) {
            menu_selection_ ^= 1U;
            render();
        } else if (input.primary_short) {
            if (menu_selection_ == 1U) return UpdateResult::ExitRequested;
            if (state_ == State::Paused) {
                state_ = rules::resume(state_);
                last_tick_ms_ = now;
                last_frame_ms_ = now;
                render();
            } else {
                start_round(now);
            }
        }
    } else if (input.primary_short || input.secondary_short) {
        return UpdateResult::ExitRequested;
    }
    return UpdateResult::Running;
}
void MiniRacerApp::tick(std::uint32_t now) {
    std::uint32_t remaining = std::min<std::uint32_t>(now - last_tick_ms_, 200U);
    last_tick_ms_ = now;
    bool changed = false;
    while (remaining != 0U && state_ == State::Playing) {
        const std::uint32_t dt = std::min(remaining, kMaxStepMs);
        remaining -= dt;
        active_play_ms_ += dt;
        score_ = active_play_ms_ / 100U;
        record_ = std::max(record_, score_);
        const std::uint32_t speed = rules::speed_for_score(score_);
        const std::uint32_t displacement = motion_.advance(speed, dt);
        if (rules::advance(enemies_, displacement, player_lane_, rng_)) {
            state_ = rules::hit(state_);
            menu_selection_ = 0;
            break;
        }
        divider_offset_q8_ =
            (divider_offset_q8_ + displacement) % (24U * 256U);
        changed = true;
    }
    if (state_ == State::GameOver) {
        render();
    } else if (changed && now - last_frame_ms_ >= kFramePeriodMs) {
        last_frame_ms_ = now;
        display_.note_visible_activity();
        render_playing(false);
    }
}
void MiniRacerApp::render_car(std::int16_t x, std::uint8_t lane,
                              board::DisplayColor color) {
    const std::int16_t y = kLaneCenters[lane] - 10;
    // Four independent wheels, a solid silhouette, cockpit and bumpers.
    board_.fill_rect(x + 4, y, 6, 4, board::DisplayColor::SecondaryText);
    board_.fill_rect(x + 17, y, 6, 4, board::DisplayColor::SecondaryText);
    board_.fill_rect(x + 4, y + 18, 6, 4, board::DisplayColor::SecondaryText);
    board_.fill_rect(x + 17, y + 18, 6, 4, board::DisplayColor::SecondaryText);
    board_.fill_rect(x + 2, y + 4, 24, 14, color);
    board_.fill_rect(x + 8, y + 2, 11, 18, color);
    board_.fill_rect(x + 10, y + 5, 7, 12, board::DisplayColor::Background);
    board_.fill_rect(x + 24, y + 7, 2, 8, board::DisplayColor::PrimaryText);
}
void MiniRacerApp::render_playing(bool full) {
    if (full) board_.fill_rect(0, 0, 240, 135, board::DisplayColor::Background);
    // Erase previous car silhouettes only. These bounds never touch the
    // HUD or horizontal road borders. Clamp to LCD edges to remove\n    // sprites partially exiting the left and right sides.
    const auto erase_car = [this](std::int32_t x, std::uint8_t lane) {
        const std::int32_t left = std::max<std::int32_t>(0, x);
        const std::int32_t right = std::min<std::int32_t>(240, x + kEnemyWidth);
        if (right > left) {
            board_.fill_rect(static_cast<std::int16_t>(left),
                static_cast<std::int16_t>(kLaneCenters[lane] - 10),
                static_cast<std::int16_t>(right - left), 22,
                board::DisplayColor::Background);
        }
    };
    if (!full && frame_valid_) {
        erase_car(33, drawn_player_lane_);
        for (const auto& enemy : drawn_enemies_) erase_car(enemy.x_q8 / 256, enemy.lane);
    }
    // Only the two-pixel moving lane divider needs full-width refreshing.
    board_.fill_rect(kRoadLeft, 68, kRoadRight - kRoadLeft + 1, 2,
                     board::DisplayColor::Background);
    char score[22]{};
    char record[23]{};
    std::snprintf(score, sizeof(score), "WYNIK %lu",
                  static_cast<unsigned long>(score_));
    std::snprintf(record, sizeof(record), "MAX %lu",
                  static_cast<unsigned long>(record_));
    if (full) board_.draw_text_region(5, 4, 87, 17, "MINI RACER", 1);
    if (full || score_ != last_drawn_score_) {
        board_.draw_text_region(99, 4, 71, 17, score, 1,
                                board::DisplayColor::Accent);
        board_.draw_text_region(174, 4, 64, 17, record, 1);
        last_drawn_score_ = score_;
    }
    if (full) {
        board_.draw_line(5, 24, 235, 24, board::DisplayColor::SecondaryText);
        board_.draw_line(kRoadLeft, 28, kRoadRight, 28, board::DisplayColor::PrimaryText);
        board_.draw_line(kRoadLeft, 110, kRoadRight, 110, board::DisplayColor::PrimaryText);
    }
    for (std::int16_t x = 5 - static_cast<std::int16_t>(divider_offset_q8_ / 256U);
         x < 235; x += 24) {
        const auto left = std::max<std::int16_t>(x, 5);
        const auto right = std::min<std::int16_t>(
            static_cast<std::int16_t>(x + 12), 235);
        if (right > left) board_.fill_rect(left, 68, right-left, 2,
                                           board::DisplayColor::SecondaryText);
    }
    render_car(33, player_lane_, board::DisplayColor::Accent);
    for (const auto& vehicle : enemies_) {
        const std::int32_t enemy = vehicle.x_q8 / 256;
        if (enemy < kRoadRight && enemy + kEnemyWidth > kRoadLeft) {
            render_car(static_cast<std::int16_t>(enemy), vehicle.lane,
                       board::DisplayColor::SecondaryText);
        }
    }
    drawn_player_lane_ = player_lane_;
    drawn_enemies_ = enemies_;
    frame_valid_ = true;
    if (full) {
        board_.draw_text_region(5, 120, 120, 13, "M5 ZMIANA PASA", 1);
        board_.draw_text_region(140, 120, 96, 13, "SIDE PAUZA", 1);
    }
}
void MiniRacerApp::render_menu(bool game_over) {
    board_.clear_screen();
    board_.draw_text_region(5, 3, 94, 15, "MINI RACER", 1);
    char summary[30]{};
    std::snprintf(summary, sizeof(summary), "WYNIK %lu  MAX %lu",
                  static_cast<unsigned long>(score_),
                  static_cast<unsigned long>(record_));
    board_.draw_text_region(85, 3, 153, 15, summary, 1);
    board_.draw_line(5, 22, 235, 22, board::DisplayColor::SecondaryText);
    board_.draw_text_region(22, 30, 206, 24,
         game_over ? "KONIEC GRY" : "PAUZA", 2);
    if (game_over) {
        char result[30]{};
        std::snprintf(result, sizeof(result), "WYNIK: %lu  MAX: %lu",
                      static_cast<unsigned long>(score_),
                      static_cast<unsigned long>(record_));
        board_.draw_text_region(22, 56, 216, 16, result, 1,
                                 board::DisplayColor::Accent);
    }
    board_.draw_text_region(22, 76, 210, 23,
          game_over ? "ZAGRAJ PONOWNIE" : "WZNÓW GRĘ", 2,
          menu_selection_ == 0 ? board::DisplayColor::Background
                                : board::DisplayColor::PrimaryText,
          menu_selection_ == 0 ? board::DisplayColor::Accent
                                : board::DisplayColor::Background);
    board_.draw_text_region(22, 101, 210, 23, "WYJDŹ", 2,
          menu_selection_ == 1 ? board::DisplayColor::Background
                                : board::DisplayColor::PrimaryText,
          menu_selection_ == 1 ? board::DisplayColor::Accent
                                : board::DisplayColor::Background);
    board_.draw_text_region(12, 125, 224, 10, "SIDE ZMIANA   M5 OK", 1);
}
void MiniRacerApp::render() {
    switch(state_) {
    case State::Playing: render_playing(); break;
    case State::Paused: render_menu(false); break;
    case State::GameOver: render_menu(true); break;
    case State::TimeLimit:
        board_.clear_screen();
        board_.draw_text_region(18, 36, 218, 30, "KONIEC SESJI", 2);
        board_.draw_text_region(18, 77, 218, 21, "10 MIN GRY", 2);
        board_.draw_text_region(18, 112, 218, 17, "M5 / SIDE: POWRÓT", 1);
        break;
    }
}
}
