#include "mini_racer/mini_racer_app.hpp"
#include <algorithm>
#include <cstdio>
#include "esp_random.h"
#include "esp_timer.h"
namespace nikos::mini_racer {
namespace {
constexpr std::uint32_t kInitialSpeedQ8 = 36U * 256U;
constexpr std::uint32_t kMaxSpeedQ8 = 70U * 256U;
constexpr std::uint32_t kAccelerationQ8 = 3U * 256U;
constexpr std::uint32_t kFramePeriodMs = 75;
constexpr std::uint32_t kMaxStepMs = 60;
constexpr std::uint32_t kMinGapPx = 80;
constexpr std::int16_t kLaneCenters[2] = {49, 90};
}
std::uint32_t MiniRacerApp::now_ms() {
    return static_cast<std::uint32_t>(esp_timer_get_time() / 1000ULL);
}
void MiniRacerApp::begin() {
    active_ = true;
    suspended_ = false;
    session_started_ms_ = now_ms();
    record_ = record_; // Record intentionally persists within this app instance.
    start_round(session_started_ms_);
}
void MiniRacerApp::start_round(std::uint32_t now) {
    state_ = State::Playing;
    player_lane_ = 1;
    enemy_lane_ = static_cast<std::uint8_t>(esp_random() & 1U);
    enemy_x_q8_ = kSpawnX * 256;
    active_play_ms_ = 0;
    score_ = 0;
    menu_selection_ = 0;
    divider_offset_ = 0;
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
            state_ = State::Paused;
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
                state_ = State::Playing;
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
        const std::uint32_t speed = std::min(
            kMaxSpeedQ8, kInitialSpeedQ8 +
                (active_play_ms_ / 10000U) * kAccelerationQ8);
        const std::int32_t old_x = enemy_x_q8_ / 256;
        enemy_x_q8_ -= static_cast<std::int32_t>((speed * dt) / 1000U);
        const std::int32_t new_x = enemy_x_q8_ / 256;
        // Swept AABB overlap prevents tunnelling on delayed frames.
        if (enemy_lane_ == player_lane_
            && old_x + kEnemyWidth >= kPlayerLeft
            && new_x <= kPlayerRight) {
            state_ = State::GameOver;
            menu_selection_ = 0;
            break;
        }
        if (new_x + kEnemyWidth < kRoadLeft) {
            // Only one opponent exists. Its next spawn always starts beyond
            // the right edge, so there is >200 px of warning at <=70 px/s.
            enemy_x_q8_ = static_cast<std::int32_t>(
                kSpawnX + kMinGapPx) * 256;
            enemy_lane_ = static_cast<std::uint8_t>(esp_random() & 1U);
        }
        divider_offset_ = static_cast<std::uint16_t>(
            (divider_offset_ + speed * dt / 1000U / 256U) % 24U);
        changed = true;
    }
    if (state_ == State::GameOver) {
        render();
    } else if (changed && now - last_frame_ms_ >= kFramePeriodMs) {
        last_frame_ms_ = now;
        display_.note_visible_activity();
        render_playing();
    }
}
void MiniRacerApp::render_car(std::int16_t x, std::uint8_t lane,
                              board::DisplayColor color) {
    const std::int16_t y = kLaneCenters[lane] - 9;
    board_.fill_rect(x, y + 3, 26, 13, color);
    board_.fill_rect(x + 6, y, 14, 19, color);
    board_.fill_rect(x + 6, y + 5, 12, 9, board::DisplayColor::Background);
    board_.fill_rect(x + 3, y - 2, 6, 3, board::DisplayColor::SecondaryText);
    board_.fill_rect(x + 17, y + 18, 6, 3, board::DisplayColor::SecondaryText);
}
void MiniRacerApp::render_playing() {
    board_.fill_rect(0, 0, 240, 135, board::DisplayColor::Background);
    char score[22]{};
    char record[23]{};
    std::snprintf(score, sizeof(score), "WYNIK %lu",
                  static_cast<unsigned long>(score_));
    std::snprintf(record, sizeof(record), "MAX %lu",
                  static_cast<unsigned long>(record_));
    board_.draw_text_region(5, 4, 87, 17, "MINI RACER", 1);
    board_.draw_text_region(99, 4, 71, 17, score, 1,
                            board::DisplayColor::Accent);
    board_.draw_text_region(174, 4, 64, 17, record, 1);
    board_.draw_line(5, 24, 235, 24, board::DisplayColor::SecondaryText);
    board_.draw_line(kRoadLeft, 28, kRoadRight, 28,
                      board::DisplayColor::PrimaryText);
    board_.draw_line(kRoadLeft, 110, kRoadRight, 110,
                      board::DisplayColor::PrimaryText);
    for (std::int16_t x = 5 - static_cast<std::int16_t>(divider_offset_);
         x < 235; x += 24) {
        const auto left = std::max<std::int16_t>(x, 5);
        const auto right = std::min<std::int16_t>(
            static_cast<std::int16_t>(x + 12), 235);
        if (right > left) board_.fill_rect(left, 68, right-left, 2,
                                           board::DisplayColor::SecondaryText);
    }
    render_car(33, player_lane_, board::DisplayColor::Accent);
    const std::int32_t enemy = enemy_x_q8_ / 256;
    if (enemy < kRoadRight && enemy + kEnemyWidth > kRoadLeft) {
        render_car(static_cast<std::int16_t>(enemy), enemy_lane_,
                   board::DisplayColor::SecondaryText);
    }
    board_.draw_text_region(5, 120, 120, 13, "M5 ZMIANA PASA", 1);
    board_.draw_text_region(140, 120, 96, 13, "SIDE PAUZA", 1);
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
    board_.draw_text_region(32, 79, 192, 20,
          game_over ? "Zagraj ponownie" : "Wznow gre", 2,
          menu_selection_ == 0 ? board::DisplayColor::Background
                                : board::DisplayColor::PrimaryText,
          menu_selection_ == 0 ? board::DisplayColor::Accent
                                : board::DisplayColor::Background);
    board_.draw_text_region(32, 101, 192, 20, "Wyjdz", 2,
          menu_selection_ == 1 ? board::DisplayColor::Background
                                : board::DisplayColor::PrimaryText,
          menu_selection_ == 1 ? board::DisplayColor::Accent
                                : board::DisplayColor::Background);
    board_.draw_text_region(5, 124, 230, 10, "SIDE ZMIANA  M5 OK", 1);
}
void MiniRacerApp::render() {
    switch(state_) {
    case State::Playing: render_playing(); break;
    case State::Paused: render_menu(false); break;
    case State::GameOver: render_menu(true); break;
    case State::TimeLimit:
        board_.clear_screen();
        board_.draw_text_region(24, 32, 200, 27, "CZAS NA PRZERWE", 2);
        board_.draw_text_region(24, 82, 200, 20, "M5 / SIDE: POWROT", 1);
        break;
    }
}
}
