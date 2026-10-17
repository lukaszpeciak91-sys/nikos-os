#include "space_defender/space_defender_app.hpp"
#include <algorithm>
#include <cstdio>
#include "esp_random.h"
#include "esp_timer.h"
namespace nikos::space_defender {
namespace {
constexpr std::uint32_t kFrameMs = 55;
board::DisplayColor enemy_color(rules::Type t) {
    return t == rules::Type::Scout ? board::DisplayColor::SecondaryText
        : (t == rules::Type::Armored ? board::DisplayColor::Attention
                                    : board::DisplayColor::Danger);
}
void clipped_rect(board::Board& board, int x, int y, int width, int height,
                  board::DisplayColor color) {
    const int left = std::max(0, x);
    const int right = std::min(240, x + width);
    const int top = std::max(27, y);
    const int bottom = std::min(112, y + height);
    if (right > left && bottom > top)
        board.fill_rect(left, top, right-left, bottom-top, color);
}

}
std::uint32_t SpaceDefenderApp::now_ms() {
    return static_cast<std::uint32_t>(esp_timer_get_time()/1000ULL);
}
void SpaceDefenderApp::begin() {
    active_=true; suspended_=false;
    session_started_ms_=now_ms();
    new_round(session_started_ms_);
}
void SpaceDefenderApp::new_round(std::uint32_t now) {
    saved_record_=std::max(saved_record_,world_.record);
    rules::reset(world_,esp_random(),saved_record_);
    menu_selection_=0;
    frame_valid_=false; last_tick_ms_=now; last_frame_ms_=now;
    render(true);
}
void SpaceDefenderApp::resume() {
    if (!active_) return;
    suspended_=false; last_tick_ms_=now_ms(); last_frame_ms_=last_tick_ms_;
    frame_valid_=false; redraw();
}
void SpaceDefenderApp::redraw() {
    if (active_) { frame_valid_=false; render(true); }
}
SpaceDefenderApp::Result SpaceDefenderApp::update(const board::InputState& input) {
    if (!active_ || input.secondary_long) return Result::ExitRequested;
    if (suspended_) return Result::Running;
    const auto now=now_ms();
    if (world_.phase!=rules::Phase::TimeLimit
        && now-session_started_ms_>=rules::kSessionMs) {
        world_.phase=rules::Phase::TimeLimit;
        render(true);
    }
    if (world_.phase==rules::Phase::Playing) {
        if (input.secondary_short) {
            world_.phase=rules::Phase::Paused;
            menu_selection_=0;render(true);
        } else {
            if (input.primary_short) rules::cycle_lane(world_);
            const auto elapsed=now-last_tick_ms_;
            last_tick_ms_=now;
            rules::advance(world_,elapsed);
            if (world_.phase==rules::Phase::GameOver) {
                saved_record_=std::max(saved_record_,world_.record);
                menu_selection_=0;render(true);
            } else if (input.primary_short || now-last_frame_ms_>=kFrameMs) {
                last_frame_ms_=now;
                display_.note_visible_activity();
                render(false);
            }
        }
    } else if (world_.phase==rules::Phase::Paused ||
               world_.phase==rules::Phase::GameOver) {
        if (input.secondary_short) {menu_selection_^=1U;render(true);}
        else if (input.primary_short) {
            if (menu_selection_==1) return Result::ExitRequested;
            if (world_.phase==rules::Phase::Paused) {
                world_.phase=rules::Phase::Playing;
                last_tick_ms_=now;last_frame_ms_=now;render(true);
            } else new_round(now);
        }
    } else if (input.primary_short || input.secondary_short) {
        return Result::ExitRequested;
    }
    return Result::Running;
}
void SpaceDefenderApp::draw_ship(std::uint8_t lane,bool visible) {
    if (!visible) return;
    const int y=rules::kLaneY[lane];
    board_.fill_rect(19,y-4,15,9,board::DisplayColor::Accent);
    board_.draw_line(33,y-4,42,y,board::DisplayColor::Accent);
    board_.draw_line(42,y,33,y+4,board::DisplayColor::Accent);
    board_.fill_rect(16,y-2,3,5,board::DisplayColor::PrimaryText);
}
void SpaceDefenderApp::draw_enemy(const rules::Enemy& e) {
    const auto x = static_cast<int>(e.x_q8 / 256);
    const auto y = rules::kLaneY[e.lane];
    const auto color = enemy_color(e.type);
    if (e.type == rules::Type::Scout) {
        clipped_rect(board_, x, y-4, 15, 9, color);
        clipped_rect(board_, x+3, y-6, 6, 13, color);
    } else if (e.type == rules::Type::Armored) {
        clipped_rect(board_, x, y-7, 20, 15, color);
        clipped_rect(board_, x+2, y-3, 16, 2, board::DisplayColor::SecondaryText);
    } else {
        clipped_rect(board_, x, y-5, 18, 11, color);
        clipped_rect(board_, x-4, y-2, 5, 5, board::DisplayColor::PrimaryText);
        if (e.warning_ms)
            clipped_rect(board_, x-9, y, 5, 2, board::DisplayColor::Accent);
    }
}
void SpaceDefenderApp::render_game(bool full) {
    const bool complete = full || !frame_valid_;
    if (complete) {
        board_.clear_screen();
        board_.draw_line(3,26,237,26,board::DisplayColor::SecondaryText);
        board_.draw_line(3,112,237,112,board::DisplayColor::SecondaryText);
        last_hud_score_=0xFFFFFFFFU;last_hud_lives_=255;
    }
    // Each lane is a small 240x22 redraw band. Clearing entire active
    // bands avoids z-order erase damage from overlapping sprites; restoring
    // their decorative stars keeps the static scene intact. No framebuffer.
    bool dirty[3] = {complete, complete, complete};
    dirty[world_.lane] = true;
    if (frame_valid_) dirty[previous_.lane] = true;
    for (const auto& e : world_.enemies) if (e.active) dirty[e.lane]=true;
    for (const auto& e : previous_.enemies) if (e.active) dirty[e.lane]=true;
    for (const auto& shot : world_.shots) if (shot.active) dirty[shot.lane]=true;
    for (const auto& shot : previous_.shots) if (shot.active) dirty[shot.lane]=true;
    for (const auto& shot : world_.hostile) if (shot.active) dirty[shot.lane]=true;
    for (const auto& shot : previous_.hostile) if (shot.active) dirty[shot.lane]=true;
    for (std::uint8_t lane=0; lane<3; ++lane) {
        if (!dirty[lane]) continue;
        const int y=rules::kLaneY[lane];
        clipped_rect(board_,0,y-11,240,22,board::DisplayColor::Background);
        // Two sparse immutable stars per lane, below all gameplay sprites.
        const int sx1=12+static_cast<int>(lane)*23;
        const int sx2=184-static_cast<int>(lane)*31;
        clipped_rect(board_,sx1,y-9,2,2,board::DisplayColor::SecondaryText);
        clipped_rect(board_,sx2,y+7,2,2,board::DisplayColor::SecondaryText);
    }
    const bool visible=!world_.invulnerable_until_ms ||
        world_.time_ms>=world_.invulnerable_until_ms ||
        (world_.time_ms/140U)%2U==0U;
    if (dirty[world_.lane]) draw_ship(world_.lane,visible);
    for (const auto& e:world_.enemies) if (e.active && dirty[e.lane] &&
        e.x_q8>-24*256 && e.x_q8<240*256) draw_enemy(e);
    for (const auto& shot:world_.shots) if (shot.active && dirty[shot.lane])
        clipped_rect(board_,shot.x_q8/256,rules::kLaneY[shot.lane]-1,
                     8,3,board::DisplayColor::Accent);
    for (const auto& shot:world_.hostile) if (shot.active && dirty[shot.lane])
        clipped_rect(board_,shot.x_q8/256,rules::kLaneY[shot.lane]-2,
                     6,5,board::DisplayColor::Danger);
    if (world_.score!=last_hud_score_ || world_.lives!=last_hud_lives_ || full) {
        char score[24]{},record[24]{};
        std::snprintf(score,sizeof(score),"WYNIK %lu",
            static_cast<unsigned long>(world_.score));
        std::snprintf(record,sizeof(record),"REKORD %lu",
            static_cast<unsigned long>(world_.record));
        board_.draw_text_region(5,4,100,16,score,1,board::DisplayColor::PrimaryText);
        board_.draw_text_region(151,4,87,16,record,1,board::DisplayColor::SecondaryText);
        board_.fill_rect(110,5,36,12,board::DisplayColor::Background);
        for (std::uint8_t i=0;i<3;++i)
            board_.fill_rect(111+i*12,8,8,6,i<world_.lives
                ? board::DisplayColor::Accent:board::DisplayColor::Surface);
        last_hud_score_=world_.score;last_hud_lives_=world_.lives;
    }
    previous_=world_;
    previous_ship_visible_=visible;
    frame_valid_=true;
}
void SpaceDefenderApp::render_menu() {
    board_.clear_screen();
    if (world_.phase==rules::Phase::TimeLimit) {
        board_.draw_text_region(16,25,208,28,"KONIEC SESJI",2);
        board_.draw_text_region(16,64,208,23,"10 MIN GRY",2);
        board_.draw_text_region(15,89,210,20,"M5 / SIDE: POWRÓT",1);
        return;
    }
    const bool over=world_.phase==rules::Phase::GameOver;
    board_.draw_text_region(14,10,212,26,over?"KONIEC GRY":"PAUZA",2);
    char score[34]{};
    std::snprintf(score,sizeof(score),"WYNIK %lu  REKORD %lu",
        static_cast<unsigned long>(world_.score),
        static_cast<unsigned long>(world_.record));
    board_.draw_text_region(12,41,220,17,score,1,board::DisplayColor::Accent);
    board_.draw_text_region(18,65,208,25,over?"ZAGRAJ PONOWNIE":"WZNÓW GRĘ",2,
        menu_selection_==0?board::DisplayColor::PrimaryText:board::DisplayColor::SecondaryText,
        menu_selection_==0?board::DisplayColor::Surface:board::DisplayColor::Background);
    board_.draw_text_region(18,93,208,25,"WYJDŹ",2,
        menu_selection_==1?board::DisplayColor::PrimaryText:board::DisplayColor::SecondaryText,
        menu_selection_==1?board::DisplayColor::Surface:board::DisplayColor::Background);
    board_.draw_text_region(14,122,208,12,"SIDE ZMIANA  M5 OK",1);
}
void SpaceDefenderApp::render(bool full) {
    if(world_.phase==rules::Phase::Playing) render_game(full);
    else render_menu();
}
}
