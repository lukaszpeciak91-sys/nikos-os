#!/usr/bin/env python3
"""R6: run production Date Editor/Calendar input paths against a recording RTC."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
src = (root / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
header = (root / "components/launcher/include/launcher/launcher.hpp").read_text(encoding="utf-8")


def section(start: str, end: str) -> str:
    count = src.count(start)
    assert count == 1, (start, count)
    return src.split(start, 1)[1].split(end, 1)[0]


def method(name: str, args: str = "()") -> str:
    original = f"void Launcher::{name}{args}"
    return f"void Harness::{name}{args}" + section(original, "\n}\n") + "\n}\n"


clock = section(
    "    if (screen_ == Screen::Clock) {\n        if (input.secondary_long)",
    "\n    if (screen_ == Screen::WorldClock) {",
)
assert "date_confirm_origin_ = Screen::Clock;" in clock
assert "confirmation_selection_ = 1U;" in clock
assert "screen_ = Screen::ClockDateConfirm;" in clock

date_confirm = "    if (screen_ == Screen::ClockDateConfirm) {" + section(
    "    if (screen_ == Screen::ClockDateConfirm) {",
    "\n    if (screen_ == Screen::WorldClock) {",
)
date_flow = (
    "    if (screen_ == Screen::ClockSetDateDay\n"
    "        || screen_ == Screen::ClockSetDateMonth\n"
    "        || screen_ == Screen::ClockSetDateYear) {"
) + section(
    "    if (screen_ == Screen::ClockSetDateDay\n"
    "        || screen_ == Screen::ClockSetDateMonth\n"
    "        || screen_ == Screen::ClockSetDateYear) {",
    "\n    if (screen_ == Screen::Calendar) {",
)
calendar = "    if (screen_ == Screen::Calendar) {" + section(
    "    if (screen_ == Screen::Calendar) {",
    "\n    if (screen_ == Screen::Settings) {",
)
assert "date_confirm_origin_ = Screen::Calendar;" in calendar
assert "return_from_date_editor(false);" in date_flow
assert "return_from_date_editor(true);" in date_flow
assert date_flow.count("return_from_date_editor(false);") == 3
assert date_flow.count("return_from_date_editor(true);") == 1
assert "clock_service_.set_date(requested)" in date_flow
assert "clock::DateSetResult::TimeUntrusted" in date_flow
assert "Screen::ClockDateSetFailed" in date_flow
assert "Screen::ClockDateNeedsTime" in date_flow
assert "void return_from_date_editor(bool saved);" in header
assert "void begin_date_editor();" in header

# Compile the unmodified input handlers and return/calendar methods from
# launcher.cpp. The mock RTC captures writes, readbacks and forced failures.
cpp = r"""
#include <cassert>
#include <cstdint>
namespace nikos {
namespace clock {
struct Date {
    std::uint16_t year=2026;
    std::uint8_t month=10, day=24;
};
struct DateReading { bool valid=true; Date date{}; };
enum class DateSetResult { Success, TimeUntrusted, Failed };
struct ClockService {
    DateReading persisted{};
    DateSetResult next=DateSetResult::Success;
    int writes=0, reads=0;
    Date last_requested{};
    DateReading read_date() { ++reads; return persisted; }
    DateSetResult set_date(Date requested) {
        ++writes;
        last_requested=requested;
        if (next==DateSetResult::Success) {
            persisted.valid=true;
            persisted.date=requested;
        }
        return next;
    }
    static std::uint8_t days_in_month(std::uint16_t year, std::uint8_t month) {
        if (month==2) return year%4==0 ? 29 : 28;
        return month==4 || month==6 || month==9 || month==11 ? 30 : 31;
    }
};
}
namespace launcher {
enum class Action { None };
struct InputState {
    bool primary_short=false, secondary_short=false, secondary_long=false;
};
constexpr std::uint8_t kClockMenuItemCount=7, kClockVisibleRows=2;
struct Harness {
    enum class Screen : std::uint8_t {
        Clock, Calendar, ClockDateConfirm, ClockSetDateDay, ClockSetDateMonth,
        ClockSetDateYear, ClockDateSetFailed, ClockDateNeedsTime
    };
    clock::ClockService clock_service_{};
    Screen screen_=Screen::Clock;
    Screen date_confirm_origin_=Screen::Clock;
    std::uint8_t clock_selection_=5, clock_view_start_=0;
    std::uint8_t confirmation_selection_=1;
    std::uint8_t calendar_action_selection_=0;
    bool calendar_initialized_=false;
    std::uint16_t calendar_year_=2026;
    std::uint8_t calendar_month_=1;
    std::uint16_t edit_year_=2026;
    std::uint8_t edit_month_=1, edit_day_=1;
    int renders_=0, moves_=0;
    void render() { ++renders_; }
    void update_clock_viewport();
    void return_from_date_editor(bool saved);
    void begin_date_editor();
    void clamp_edit_date_day();
    void begin_calendar();
    void move_calendar_month(bool) { ++moves_; }
    Action update(const InputState& input);
};
"""
cpp += method("update_clock_viewport")
cpp += method("return_from_date_editor", "(bool saved)")
cpp += method("begin_date_editor")
cpp += method("clamp_edit_date_day")
cpp += method("begin_calendar")
cpp += """
Action Harness::update(const InputState& input) {
""" + date_confirm + date_flow + calendar + """
    return Action::None;
}
}
}
using nikos::launcher::Harness;
using nikos::launcher::InputState;
using nikos::clock::DateSetResult;
InputState m5() { InputState i{}; i.primary_short=true; return i; }
InputState side() { InputState i{}; i.secondary_short=true; return i; }
InputState back() { InputState i{}; i.secondary_long=true; return i; }
void enter_clock_confirm(Harness& h) {
    h.screen_=Harness::Screen::ClockDateConfirm;
    h.date_confirm_origin_=Harness::Screen::Clock;
    h.clock_selection_=5;
    h.confirmation_selection_=1;
}
void enter_calendar_confirm(Harness& h) {
    h.clock_service_.persisted.valid=false;
    h.begin_calendar();
    assert(!h.calendar_initialized_);
    h.update(m5()); // Real Calendar handler -> confirmation screen.
    assert(h.screen_==Harness::Screen::ClockDateConfirm);
    assert(h.date_confirm_origin_==Harness::Screen::Calendar);
    assert(h.confirmation_selection_==1);
}
void accept(Harness& h) {
    h.update(side()); // Default NIE -> TAK
    assert(h.confirmation_selection_==0);
    h.update(m5());
    assert(h.screen_==Harness::Screen::ClockSetDateDay);
}
void reach_year(Harness& h) {
    h.update(m5());
    assert(h.screen_==Harness::Screen::ClockSetDateMonth);
    h.update(m5());
    assert(h.screen_==Harness::Screen::ClockSetDateYear);
}
void assert_origin(Harness& h, bool from_calendar) {
    if (from_calendar) {
        assert(h.screen_==Harness::Screen::Calendar);
    } else {
        assert(h.screen_==Harness::Screen::Clock);
        assert(h.clock_selection_==5);
        assert(h.clock_view_start_<=5);
    }
}
int main() {
    using Screen=Harness::Screen;

    for (bool calendar : {false, true}) {
        // Default NO and SIDE-long confirmation cancellation must remain safe.
        {
            Harness h;
            if (calendar) enter_calendar_confirm(h);
            else enter_clock_confirm(h);
            const int before=h.clock_service_.writes;
            h.update(m5());
            assert_origin(h, calendar);
            assert(h.clock_service_.writes==before);
        }
        {
            Harness h;
            if (calendar) enter_calendar_confirm(h);
            else enter_clock_confirm(h);
            h.update(back());
            assert_origin(h, calendar);
            assert(h.clock_service_.writes==0);
        }
        // SIDE long from each of the three editing stages: no RTC mutation.
        for (unsigned stage=0; stage<3; ++stage) {
            Harness h;
            if (calendar) enter_calendar_confirm(h);
            else enter_clock_confirm(h);
            accept(h);
            const auto initial=h.clock_service_.persisted.date;
            const int reads_before=h.clock_service_.reads;
            h.update(side()); // Stage a modified day (unconfirmed).
            if(stage>=1) h.update(m5());
            if(stage>=2) h.update(m5());
            h.update(back());
            assert_origin(h, calendar);
            assert(h.clock_service_.writes==0);
            assert(h.clock_service_.persisted.date.day==initial.day);
            assert(h.clock_service_.persisted.date.month==initial.month);
            assert(h.clock_service_.persisted.date.year==initial.year);
            assert(h.clock_service_.reads==reads_before); // No refresh on cancel.
            if (calendar) {
                assert(!h.calendar_initialized_);
                assert(h.calendar_action_selection_==0);
            }
        }
        // Successful date save returns to the origin. Calendar must refresh
        // its displayed month and validity by reading saved RTC state.
        {
            Harness h;
            if (calendar) enter_calendar_confirm(h);
            else enter_clock_confirm(h);
            accept(h);
            h.update(side()); // 25 October or 2 January
            h.update(m5());
            h.update(side()); // November or February
            h.update(m5());
            h.update(side()); // 2027
            const int reads_before=h.clock_service_.reads;
            h.update(m5()); // Actual production RTC commit.
            assert_origin(h, calendar);
            assert(h.clock_service_.writes==1);
            assert(h.clock_service_.persisted.valid);
            assert(h.clock_service_.persisted.date.year==2027);
            assert(h.clock_service_.persisted.date.month==(calendar ? 2 : 11));
            assert(h.clock_service_.persisted.date.day==(calendar ? 2 : 25));
            if (calendar) {
                assert(h.clock_service_.reads==reads_before+1);
                assert(h.calendar_initialized_);
                assert(h.calendar_year_==2027);
                assert(h.calendar_month_==2);
                assert(h.calendar_action_selection_==0);
                h.update(side()); // Fully initialized 3-action Calendar.
                assert(h.calendar_action_selection_==1);
            } else {
                assert(h.clock_service_.reads==reads_before);
            }
        }
        // Both error results; dismissal using all three inputs must restore
        // the origin but never call RTC a second time or refresh an invalid date.
        for (DateSetResult failure : {DateSetResult::Failed,
                                       DateSetResult::TimeUntrusted}) {
            for (int dismiss=0; dismiss<3; ++dismiss) {
                Harness h;
                if (calendar) enter_calendar_confirm(h);
                else enter_clock_confirm(h);
                h.clock_service_.next=failure;
                accept(h);
                reach_year(h);
                h.update(m5());
                assert(h.screen_==(failure==DateSetResult::TimeUntrusted
                        ? Screen::ClockDateNeedsTime : Screen::ClockDateSetFailed));
                assert(h.clock_service_.writes==1);
                const int reads_before=h.clock_service_.reads;
                if(dismiss==0) h.update(m5());
                if(dismiss==1) h.update(side());
                if(dismiss==2) h.update(back());
                assert_origin(h,calendar);
                assert(h.clock_service_.writes==1);
                assert(h.clock_service_.reads==reads_before);
                assert(h.clock_service_.persisted.valid==!calendar);
                if(calendar) assert(!h.calendar_initialized_);
            }
        }
    }
    return 0;
}
"""
with tempfile.TemporaryDirectory() as temp:
    source=Path(temp)/"date_editor_r6.cpp"
    binary=Path(temp)/"date_editor_r6"
    source.write_text(cpp, encoding="utf-8")
    subprocess.run([
        "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        str(source), "-o", str(binary),
    ], check=True)
    subprocess.run([str(binary)], check=True)
print("Clock R6 editor origin, RTC safety, Calendar refresh and errors PASS")
