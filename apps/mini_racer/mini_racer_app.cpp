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
    start_round(now_ms());
}
void MiniRacerApp::start_round(std::uint32_t now) {
    state_ = State::Playing;
    round_.begin(now);
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
    // Deadline belongs to this attempt, including pause and suspension.
    // A completed finish or collision remains the final round outcome.
    if ((state_ == State::Playing || state_ == State::Paused)
        && round_.deadline(now)) {
        round_.advance(0U,false,now);
        state_ = State::TimeLimit;
        menu_selection_ = 1;
        render();
    }
    if (state_ == State::FinishCrossing) {
        if (now-crossing_started_ms_ >= 650U) {
            state_ = State::Victory;
            render();
        }
        return UpdateResult::Running;
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
    } else if (state_ == State::Victory || state_ == State::TimeLimit) {
        if (input.primary_short) start_round(now);
        else if (input.secondary_short) return UpdateResult::ExitRequested;
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
        const std::uint32_t speed = race::stage_speed(
            rules::speed_for_score(score_),round_.stage());
        const std::uint32_t displacement = motion_.advance(speed, dt);
        const bool collision=rules::advance(
            enemies_,displacement,player_lane_,rng_);
        round_.advance(displacement,collision,now);
        if (collision) {
            state_ = rules::hit(state_);
            menu_selection_ = 0;
            break;
        }
        if (round_.outcome==race::Outcome::Victory) {
            state_=State::FinishCrossing;
            crossing_started_ms_=now;
            break;
        }
        divider_offset_q8_ =
            (divider_offset_q8_ + displacement) % (24U * 256U);
        changed = true;
    }
    if (state_ == State::GameOver) {
        render();
    } else if (state_ == State::FinishCrossing) {
        render_finish_crossing();
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
    // Keep the two-lane road unchanged. Progress occupies the narrow
    // strip between road and navigation footer, refreshed only on redraw.
    board_.fill_rect(5,113,230,7,board::DisplayColor::Background);
    const std::int16_t bar_width=static_cast<std::int16_t>(
        round_.progress_percent()*145U/100U);
    board_.fill_rect(5,115,bar_width,4,board::DisplayColor::Accent);
    char stage[22]{};
    std::snprintf(stage,sizeof(stage),"ETAP %u  %u%%",
        static_cast<unsigned>(round_.stage()),
        static_cast<unsigned>(round_.progress_percent()));
    board_.draw_text_region(157,112,80,9,stage,1,
        board::DisplayColor::SecondaryText);
    if (full) {
        board_.draw_text_region(5, 122, 120, 12, "M5 ZMIANA PASA", 1);
        board_.draw_text_region(140, 122, 96, 12, "SIDE PAUZA", 1);
    }
}
void MiniRacerApp::render_menu(bool game_over) {
    if (!game_over) {
        // Space Guard's approved pause hierarchy, without sharing game logic.
        board_.clear_screen();
        board_.draw_text_region(14, 10, 212, 26, "PAUZA", 2);
        char summary[34]{};
        std::snprintf(summary, sizeof(summary), "WYNIK %lu  MAX %lu",
            static_cast<unsigned long>(score_),
            static_cast<unsigned long>(record_));
        board_.draw_text_region(12, 41, 220, 17, summary, 1,
            board::DisplayColor::Accent);
        board_.draw_text_region(18, 65, 208, 25, "WZNÓW GRĘ", 2,
            menu_selection_ == 0 ? board::DisplayColor::PrimaryText
                                 : board::DisplayColor::SecondaryText,
            menu_selection_ == 0 ? board::DisplayColor::Surface
                                 : board::DisplayColor::Background);
        board_.draw_text_region(18, 93, 208, 25, "WYJDŹ", 2,
            menu_selection_ == 1 ? board::DisplayColor::PrimaryText
                                 : board::DisplayColor::SecondaryText,
            menu_selection_ == 1 ? board::DisplayColor::Surface
                                 : board::DisplayColor::Background);
        board_.draw_text_region(12, 125, 224, 10, "SIDE ZMIANA   M5 OK", 1);
        return;
    }

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
void MiniRacerApp::render_finish_crossing() {
    render_playing(true);
    // Last stretch: explicit line at the player's nose before results appear.
    for(std::int16_t y=29;y<110;y+=10) {
        board_.fill_rect(61,y,6,5,board::DisplayColor::PrimaryText);
        board_.fill_rect(67,y+5,6,5,board::DisplayColor::Accent);
    }
}

void MiniRacerApp::render_victory() {
    board_.clear_screen();
    board_.draw_text_region(57,12,145,30,"META!",3,
        board::DisplayColor::Accent);
    board_.draw_text_region(16,49,222,24,"TRASA UKOŃCZONA",2,
        board::DisplayColor::PrimaryText);
    char result[30]{};
    const auto sec=active_play_ms_/1000U;
    std::snprintf(result,sizeof(result),"CZAS %lu:%02lu",
        static_cast<unsigned long>(sec/60U),
        static_cast<unsigned long>(sec%60U));
    board_.draw_text_region(52,76,155,17,result,1);
    board_.draw_text_region(22,97,210,17,"M5 PONOWNIE  SIDE WYJDŹ",1);
    board_.draw_text_region(22,117,210,14,"BOCZNY DŁUGO = WYJDŹ",1);
}

void MiniRacerApp::render() {
    switch(state_) {
    case State::Playing: render_playing(); break;
    case State::Paused: render_menu(false); break;
    case State::GameOver: render_menu(true); break;
    case State::FinishCrossing: render_finish_crossing(); break;
    case State::Victory: render_victory(); break;
    case State::TimeLimit:
        board_.clear_screen();
        board_.draw_text_region(18, 36, 218, 30, "KONIEC CZASU", 2);
        board_.draw_text_region(18, 77, 218, 21, "10 MIN GRY", 2);
        board_.draw_text_region(18, 112, 218, 17, "M5 PONÓW  SIDE WYJDŹ", 1);
        break;
    }
}
}
