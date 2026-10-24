#!/usr/bin/env python3
"""Clock R4: compile actual menu/confirmation/calendar handlers in a host harness.

The input handlers and menu viewport methods are extracted from production C++;
the fake clock records RTC writes, so the tests exercise real control flow.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CPP = (ROOT / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
HPP = (ROOT / "components/launcher/include/launcher/launcher.hpp").read_text(encoding="utf-8")


def section(start, end):
    assert CPP.count(start) == 1, (start, CPP.count(start))
    return CPP.split(start, 1)[1].split(end, 1)[0]


menu = section(
    "constexpr const char* kItems[kClockMenuItemCount] = {",
    "};",
)
labels = re.findall(r'"([^"]+)"|(\bnullptr\b)', menu)
labels = [left or right for left, right in labels]
assert labels == [
    "KALENDARZ", "nullptr", "STOPER", "ZEGAR ŚWIATOWY",
    "USTAW CZAS", "USTAW DATĘ", "POWRÓT",
], labels
assert "constexpr std::uint8_t kClockMenuItemCount = 7;" in CPP
assert "constexpr std::uint8_t kClockVisibleRows = 2;" in CPP
assert "constexpr std::uint8_t kClockTimerIndex = 1;" in CPP
assert "if (index == kClockTimerIndex) continue;" in CPP
timer_paint = section(
    "void Launcher::render_clock_timer_if_changed()", "\n}\n"
)
assert "kClockTimerIndex - clock_view_start_" in timer_paint
assert "clock_selection_ == kClockTimerIndex" in timer_paint
assert "kClockTimerIndex < clock_view_start_" in timer_paint
assert "const auto index = static_cast<std::uint8_t>(clock_view_start_ + slot);" in CPP
assert "ClockDateConfirm," in HPP
assert "void return_from_date_editor(bool saved);" in HPP
assert "Screen date_confirm_origin_ = Screen::Clock;" in HPP
assert "void render_date_confirm();" in HPP

clock_handler = section(
    "    if (screen_ == Screen::Clock) {\n        if (input.secondary_long)",
    "\n    if (screen_ == Screen::WorldClock) {",
)
clock_handler = (
    "    if (screen_ == Screen::Clock) {\n        if (input.secondary_long)"
    + clock_handler
)
calendar_handler = (
    "    if (screen_ == Screen::Calendar) {"
    + section(
        "    if (screen_ == Screen::Calendar) {",
        "\n    if (screen_ == Screen::Settings) {",
    )
)
viewport_method = (
    "void Harness::update_clock_viewport()"
    + section(
        "void Launcher::update_clock_viewport()",
        "\nvoid Launcher::begin_date_editor()",
    )
)
date_begin_method = (
    "void Harness::begin_date_editor()"
    + section(
        "void Launcher::begin_date_editor()",
        "\nvoid Launcher::clamp_edit_date_day()",
    )
)
calendar_begin_method = (
    "void Harness::begin_calendar()"
    + section(
        "void Launcher::begin_calendar()",
        "\nvoid Launcher::move_calendar_month(",
    )
)

# Assert retained editor/RTC code and affected returns, not just labels.
editor = section(
    "    if (screen_ == Screen::ClockSetDateDay\n        || screen_ == Screen::ClockSetDateMonth",
    "\n    if (screen_ == Screen::ClockDateSetFailed) {",
)
assert "clock_service_.set_date(requested)" in editor
assert "Screen::ClockSetDateMonth" in editor
assert "Screen::ClockSetDateYear" in editor
assert "return_from_date_editor(false);" in editor
assert "return_from_date_editor(true);" in editor
assert "return_from_date_editor(false);" in section(
    "    if (screen_ == Screen::ClockDateSetFailed)",
    "\n    if (screen_ == Screen::ClockDateNeedsTime)",
)
assert "return_from_date_editor(false);" in section(
    "    if (screen_ == Screen::ClockDateNeedsTime)",
    "\n    if (screen_ == Screen::Calendar)",
)
assert "clock_selection_ = 2;" in section(
    "void Launcher::exit_stopwatch_to_clock()",
    "\nvoid Launcher::update_clock_viewport()",
)
assert "clock_selection_ = 3U;" in section(
    "    if (screen_ == Screen::WorldClock)",
    "\n    if (screen_ == Screen::TimerSetup)",
)
assert CPP.count("clock_selection_ = 1;") == 3  # TimerSetup + 2 TimerActive exits.
assert "clock_selection_ = 4;" in section(
    "    if (screen_ == Screen::ClockSetHour\n        || screen_ == Screen::ClockSetMinute) {",
    "\n    if (screen_ == Screen::ClockSetFailed",
)
assert "clock_selection_ = 0;" in calendar_handler

confirm_render = section(
    "void Launcher::render_date_confirm()", "\nvoid Launcher::render_date_editor()"
)
for expected in (
    '"ZMIENIĆ"', '"DATĘ?"', '"TAK"', '"NIE"',
    "confirmation_selection_ == index",
    "67 + index * 26",
    '"M5 WYBIERZ | BOCZNY DALEJ"',
    "draw_pilot_text(18, 12, 204, 25",
    "draw_pilot_text(18, 39, 204, 25",
):
    assert expected in confirm_render, expected
assert "case Screen::ClockDateConfirm:" in CPP
assert "render_date_confirm();" in CPP

header = r"""
#include <cassert>
#include <cstdint>
namespace nikos {
namespace clock {
struct Time { std::uint8_t hour=13, minute=42; };
struct Reading { bool valid=true; Time time{}; };
struct Date { std::uint16_t year=2026; std::uint8_t month=10, day=24; };
struct DateReading { bool valid=true; Date date{}; };
struct ClockService {
    Reading time{};
    DateReading date{};
    unsigned rtc_writes=0;
    unsigned date_reads=0;
    Reading read() { return time; }
    DateReading read_date() { ++date_reads; return date; }
    void set_date(Date) { ++rtc_writes; }
};
}
namespace countdown {
enum class State { Idle, Running };
struct Service { State current=State::Idle; State state() const { return current; } };
}
namespace launcher {
enum class Action { None };
struct InputState { bool primary_short=false, secondary_short=false, secondary_long=false; };
struct Harness {
    enum class Screen {
        Main, Clock, ClockDateConfirm, Calendar, TimerSetup, TimerActive,
        Stopwatch, WorldClock, ClockSetHour, ClockSetDateDay
    };
    static constexpr std::uint8_t kClockMenuItemCount=7;
    static constexpr std::uint8_t kClockVisibleRows=2;
    clock::ClockService clock_service_{};
    countdown::Service countdown_{};
    Screen screen_=Screen::Clock;
    Screen date_confirm_origin_=Screen::Clock;
    std::uint8_t clock_selection_=0, clock_view_start_=0;
    std::uint8_t confirmation_selection_=0;
    std::uint8_t timer_selection_=0, world_city_=0;
    std::uint8_t edit_hour_=0, edit_minute_=0;
    std::uint8_t edit_day_=1, edit_month_=1;
    std::uint16_t edit_year_=2026, calendar_year_=2026;
    std::uint8_t calendar_month_=1, calendar_action_selection_=0;
    bool calendar_initialized_=false;
    unsigned renders_=0, stopwatch_resets_=0, month_moves_=0;
    void render() { ++renders_; }
    void reset_stopwatch_session() { ++stopwatch_resets_; }
    void move_calendar_month(bool) { ++month_moves_; }
    void update_clock_viewport();
    void begin_date_editor();
    void begin_calendar();
    Action update(const InputState& input);
};
"""
tail = r"""
Action Harness::update(const InputState& input) {
""" + clock_handler + calendar_handler + r"""
    return Action::None;
}
}
}

using nikos::launcher::Harness;
using nikos::launcher::InputState;
InputState side() { InputState i{}; i.secondary_short=true; return i; }
InputState back() { InputState i{}; i.secondary_long=true; return i; }
InputState m5() { InputState i{}; i.primary_short=true; return i; }

int main() {
    using Screen = Harness::Screen;
    Harness h;
    const auto original = h.clock_service_.date.date;
    auto unchanged = [&]() {
        assert(h.clock_service_.rtc_writes==0);
        assert(h.clock_service_.date.date.year==original.year);
        assert(h.clock_service_.date.date.month==original.month);
        assert(h.clock_service_.date.date.day==original.day);
    };
    // Clock navigation wraps all seven items, viewport stays in bounds.
    for (unsigned index=0; index<7; ++index) {
        assert(h.clock_selection_==index);
        h.update(side());
        assert(h.clock_selection_==(index+1)%7);
        assert(h.clock_view_start_<=5);
    }
    h.clock_selection_=0;
    h.update(m5());
    assert(h.screen_==Screen::Calendar && h.calendar_initialized_);
    h.update(back());
    assert(h.screen_==Screen::Clock && h.clock_selection_==0);
    for (unsigned index=1;index<=6;++index) {
        h.screen_=Screen::Clock; h.clock_selection_=index;
        h.update(m5());
        if(index==1) assert(h.screen_==Screen::TimerSetup);
        if(index==2) assert(h.screen_==Screen::Stopwatch && h.stopwatch_resets_>0);
        if(index==3) assert(h.screen_==Screen::WorldClock && h.world_city_==0);
        if(index==4) assert(h.screen_==Screen::ClockSetHour
            && h.edit_hour_==13 && h.edit_minute_==42);
        if(index==5) {
            assert(h.screen_==Screen::ClockDateConfirm);
            assert(h.confirmation_selection_==1);
        }
        if(index==6) assert(h.screen_==Screen::Main);
    }
    // Clock origin: default NO, redrawing does not select YES or edit RTC.
    h.screen_=Screen::Clock; h.clock_selection_=5;
    h.update(m5());
    assert(h.screen_==Screen::ClockDateConfirm && h.confirmation_selection_==1);
    h.render();
    h.update(m5());
    assert(h.screen_==Screen::Clock && h.clock_selection_==5);
    unchanged();
    h.update(m5());
    h.update(side());
    assert(h.confirmation_selection_==0);
    h.update(side());
    assert(h.confirmation_selection_==1);
    h.update(back());
    assert(h.screen_==Screen::Clock && h.clock_selection_==5);
    unchanged();
    h.update(m5());
    h.update(side());
    h.update(m5());
    assert(h.screen_==Screen::ClockSetDateDay);
    assert(h.edit_day_==24 && h.edit_month_==10 && h.edit_year_==2026);
    unchanged();
    // Calendar origin: missing RTC date leads to confirmation. Cancel must
    // restore the calendar and its action focus, not the Clock menu.
    h.clock_service_.date.valid=false;
    h.screen_=Screen::Clock; h.clock_selection_=0;
    h.update(m5());
    assert(h.screen_==Screen::Calendar && !h.calendar_initialized_);
    assert(h.calendar_action_selection_==0);
    h.update(m5());
    assert(h.screen_==Screen::ClockDateConfirm);
    assert(h.date_confirm_origin_==Screen::Calendar && h.confirmation_selection_==1);
    h.render();
    h.update(back());
    assert(h.screen_==Screen::Calendar && h.calendar_action_selection_==0);
    unchanged();
    h.update(m5());
    h.update(m5());  // Default NO returns to Calendar.
    assert(h.screen_==Screen::Calendar);
    unchanged();
    h.update(m5());
    h.update(side()); h.update(m5());
    assert(h.screen_==Screen::ClockSetDateDay);
    assert(h.edit_day_==1 && h.edit_month_==1 && h.edit_year_==2026);
    unchanged();
    h.screen_=Screen::Calendar;
    h.update(side());
    assert(h.calendar_action_selection_==1);
    h.update(m5());
    assert(h.screen_==Screen::Clock && h.clock_selection_==0);
    unchanged();
    return 0;
}
"""
assembled = header + viewport_method + date_begin_method + calendar_begin_method + tail

with tempfile.TemporaryDirectory() as directory:
    src = Path(directory) / "r4_clock_host.cpp"
    binary = Path(directory) / "r4_clock_host"
    src.write_text(assembled, encoding="utf-8")
    subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
         str(src), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)

print("Clock R4 real input handlers, menus, confirmation origins and RTC cancellation PASS")
