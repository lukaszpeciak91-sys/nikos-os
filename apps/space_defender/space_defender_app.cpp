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
void erase_area(board::Board& b, std::int32_t x, int y, int width, int height) {
    const int left = std::max<int>(0,static_cast<int>(x));
    const int right = std::min<int>(240,static_cast<int>(x)+width);
    if (right>left) b.fill_rect(left,y,right-left,height,board::DisplayColor::Background);
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
    const auto x=static_cast<std::int16_t>(e.x_q8/256);
    const auto y=rules::kLaneY[e.lane];
    const auto color=enemy_color(e.type);
    if (e.type==rules::Type::Scout) {
        board_.fill_rect(x,y-4,15,9,color);
        board_.fill_rect(x+3,y-6,6,13,color);
    } else if (e.type==rules::Type::Armored) {
        board_.fill_rect(x,y-7,20,15,color);
        board_.draw_line(x+2,y-3,x+18,y-3,board::DisplayColor::SecondaryText);
    } else {
        board_.fill_rect(x,y-5,18,11,color);
        board_.fill_rect(x-4,y-2,5,5,board::DisplayColor::PrimaryText);
        if (e.warning_ms) board_.draw_line(x-9,y,x-5,y,board::DisplayColor::Accent);
    }
}
void SpaceDefenderApp::render_game(bool full) {
    if (full || !frame_valid_) {
        board_.clear_screen();
        board_.draw_line(3,26,237,26,board::DisplayColor::SecondaryText);
        board_.draw_line(3,112,237,112,board::DisplayColor::SecondaryText);
        for (int x=12;x<238;x+=46) {
            board_.fill_rect(x,31,2,2,board::DisplayColor::SecondaryText);
            board_.fill_rect(x+18,108,2,2,board::DisplayColor::SecondaryText);
        }
        last_hud_score_=0xFFFFFFFFU;last_hud_lives_=255;
    } else {
        erase_area(board_,14,rules::kLaneY[previous_.lane]-9,32,18);
        for (const auto& e:previous_.enemies) if(e.active)
            erase_area(board_,e.x_q8/256-11,rules::kLaneY[e.lane]-10,36,20);
        for (const auto& s:previous_.shots) if(s.active)
            erase_area(board_,s.x_q8/256-2,rules::kLaneY[s.lane]-2,12,5);
        for (const auto& s:previous_.hostile) if(s.active)
            erase_area(board_,s.x_q8/256-2,rules::kLaneY[s.lane]-3,10,7);
    }
    const bool visible=!world_.invulnerable_until_ms ||
        world_.time_ms>=world_.invulnerable_until_ms ||
        (world_.time_ms/140U)%2U==0U;
    draw_ship(world_.lane,visible);
    for (const auto& e:world_.enemies) if(e.active &&
        e.x_q8>-24*256 && e.x_q8<240*256) draw_enemy(e);
    for (const auto& s:world_.shots) if(s.active && s.x_q8<240*256)
        board_.fill_rect(static_cast<std::int16_t>(s.x_q8/256),
            rules::kLaneY[s.lane]-1,8,3,board::DisplayColor::Accent);
    for (const auto& s:world_.hostile) if(s.active && s.x_q8>0)
        board_.fill_rect(static_cast<std::int16_t>(s.x_q8/256),
            rules::kLaneY[s.lane]-2,6,5,board::DisplayColor::Danger);
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
    previous_=world_;previous_ship_visible_=visible;frame_valid_=true;
}
void SpaceDefenderApp::render_menu() {
    board_.clear_screen();
    if (world_.phase==rules::Phase::TimeLimit) {
        board_.draw_text_region(16,37,208,28,"CZAS NA PRZERWĘ",2);
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
