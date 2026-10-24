#!/usr/bin/env python3
"""R5: Clock hints geometry and original World Clock/Timer controls on host."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
CPP = (ROOT / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
GLYPHS = (ROOT / "components/font_core/glyph_assets.hpp").read_text(encoding="utf-8")
BOARD_FONT = (ROOT / "components/board/polish_ui_font.cpp").read_text(
    encoding="utf-8"
)


def between(start, end):
    assert CPP.count(start) == 1, (start, CPP.count(start))
    return CPP.split(start, 1)[1].split(end, 1)[0]


world = between(
    "void Launcher::render_world_clock_if_changed(bool force)",
    "\nvoid Launcher::render_timer_setup()",
)
setup = between("void Launcher::render_timer_setup()", "\nvoid Launcher::render_timer_active()")
active = between("void Launcher::render_timer_active()", "\nvoid Launcher::render_stopwatch()")

# The exact intended instruction is assembled without a font-size change.
parts_body = world.split("constexpr const char* kHintParts[] = {", 1)[1].split("};", 1)[0]
parts = re.findall(r'"([^"]+)"', parts_body)
gaps_body = world.split("constexpr std::int16_t kHintGaps[] = {", 1)[1].split("};", 1)[0]
gaps = [int(n) for n in re.findall(r"\d+", gaps_body)]
assert parts == ["M5", ">", "|", "SIDE", "<", "|", "SIDE", "DŁ.", "WRÓĆ"]
assert gaps == [3, 3, 3, 3, 3, 3, 2, 2]
assert " ".join(parts) == "M5 > | SIDE < | SIDE DŁ. WRÓĆ"
assert "font_renderer_.set_size(font_core::Size::Compact);" in world
assert "font_renderer_.measure(kHintParts[i], 224, 19).width_px" in world
assert "font_renderer_.draw(kHintParts[i], {hint_x, 116, width, 19}" in world
assert "board::DisplayColor::SecondaryText" in world
advances = {
    chr(int(cp)): int(width) + 1
    for cp, width in re.findall(r'\{(\d+),\s*(\d+),\s*"[0-9a-f]+"\}', GLYPHS)
}
assert all(c in advances for part in parts for c in part)
actual_width = sum(sum(advances[c] for c in word) for word in parts) + sum(gaps)
assert actual_width == 224, actual_width
assert 8 + actual_width == 232 <= 240 and 116 + 19 == 135
assert sum(advances[c] for c in "M5 > | SIDE < | SIDE DŁ. WRÓĆ") > 224

# Timer setup retains the same x/y, font size 2 and colors.
expected = (
    ("M5 START", 63, 18, "Accent"),
    ("SIDE +CZAS", 82, 18, "SecondaryText"),
    ("SIDE DŁ. WRÓĆ", 101, 16, "SecondaryText"),
)
for label, y, height, color in expected:
    assert re.search(
        r"board_\.draw_text_region\(\s*22,\s*"
        + str(y) + r",\s*196,\s*" + str(height)
        + r',\s*"' + re.escape(label) + r'",\s*2,\s*'
        + r"board::DisplayColor::" + color,
        setup,
    ), label
assert "BOCZNY +CZAS" not in setup
assert "BOCZNY DLUGO" not in setup
assert '"= POWROT"' not in setup
assert setup.count("board_.draw_text_region(") == 5

# Native Polish glyphs have room in the existing size-2 196px timer region.
glyph_body = BOARD_FONT.split(
    "const lgfx::GFXglyph kPolishUiGlyphs[] PROGMEM = {", 1
)[1].split("};", 1)[0]
ranges = BOARD_FONT.split(
    "const lgfx::EncodeRange kPolishUiRanges[] PROGMEM = {", 1
)[1].split("};", 1)[0]
widths = [int(n) for n in re.findall(
    r"\{\s*\d+,\s*\d+,\s*\d+,\s*(\d+),", glyph_body
)]
mapped = {}
for a, b, idx in re.findall(
    r"\{\s*0x([0-9A-F]+),\s*0x([0-9A-F]+),\s*(\d+)\s*\}", ranges
):
    first, last, offset = int(a, 16), int(b, 16), int(idx)
    for cp in range(first, last + 1):
        mapped[chr(cp)] = widths[offset + cp - first]
for label, y, height, _ in expected:
    assert y + height <= 135
    assert all(ch in mapped for ch in label)
    assert sum(mapped[ch] for ch in label) * 2 <= 196, label

# Only the displayed third action is renamed; other menu items remain.
assert re.search(
    r'const char\* actions\[3\]\s*=\s*\{\s*primary_action,\s*"RESETUJ",\s*"W TLE",\s*\};',
    active,
)
assert '"POWRÓT"' not in active
assert '"PAUZA"' in active and '"WZNOW"' in active
assert 'timer_selection_ == 0U' in CPP and 'timer_selection_ == 1U' in CPP
assert CPP.count("void Launcher::render_stopwatch()") == 1

# Compile the production WorldClock, TimerSetup and TimerActive input branches
# against a recording countdown stub. Returning to Clock must never stop it.
handler = "    if (screen_ == Screen::WorldClock) {" + between(
    "    if (screen_ == Screen::WorldClock) {",
    "\n    if (screen_ == Screen::Stopwatch) {",
)
host = r"""
#include <cassert>
#include <cstdint>
namespace nikos {
namespace clock { namespace world { constexpr unsigned kCityCount = 15; } }
namespace countdown {
enum class State { Idle, Running, Paused };
struct Service {
    State current = State::Idle;
    int seconds = 120, duration = 120;
    int resets = 0, starts = 0;
    State state() const { return current; }
    void advance_configured_duration() { duration += 30; }
    bool start() { ++starts; seconds = duration; current = State::Running; return true; }
    void pause() { assert(current == State::Running); current = State::Paused; }
    void resume() { assert(current == State::Paused); current = State::Running; }
    void reset() { ++resets; current = State::Idle; }
    void tick() { if (current == State::Running) --seconds; }
};
}
namespace launcher {
enum class Action { None };
struct InputState { bool primary_short=false, secondary_short=false, secondary_long=false; };
struct Harness {
    enum class Screen { Clock, WorldClock, TimerSetup, TimerActive };
    Screen screen_ = Screen::WorldClock;
    std::uint8_t clock_selection_=3, world_city_=0, timer_selection_=0;
    countdown::Service countdown_{};
    unsigned renders_=0, world_renders_=0;
    void render() { ++renders_; }
    void update_clock_viewport() {}
    void render_world_clock_if_changed(bool = false) { ++world_renders_; }
    Action update(const InputState& input);
};
Action Harness::update(const InputState& input) {
""" + handler + r"""
    return Action::None;
}
}
}
using nikos::launcher::Harness;
using nikos::launcher::InputState;
InputState m5() { InputState i{}; i.primary_short=true; return i; }
InputState side() { InputState i{}; i.secondary_short=true; return i; }
InputState long_side() { InputState i{}; i.secondary_long=true; return i; }
int main() {
    using Screen = Harness::Screen;
    using State = nikos::countdown::State;
    Harness h;
    h.world_city_=14;
    h.update(m5());
    assert(h.world_city_==0 && h.screen_==Screen::WorldClock);
    h.update(side());
    assert(h.world_city_==14);
    h.update(long_side());
    assert(h.screen_==Screen::Clock && h.clock_selection_==3);
    h.screen_=Screen::TimerSetup;
    h.update(side());
    assert(h.countdown_.duration==150);
    h.update(long_side());
    assert(h.screen_==Screen::Clock && h.clock_selection_==1);
    h.screen_=Screen::TimerSetup;
    h.update(m5());
    assert(h.screen_==Screen::TimerActive && h.countdown_.state()==State::Running);
    h.update(side()); h.update(side());
    assert(h.timer_selection_==2);
    const int remaining = h.countdown_.seconds;
    h.update(m5());  // "W TLE".
    assert(h.screen_==Screen::Clock && h.clock_selection_==1);
    assert(h.countdown_.state()==State::Running && h.countdown_.resets==0);
    h.countdown_.tick();
    assert(h.countdown_.seconds==remaining-1); // Background still ticking.
    h.screen_=Screen::TimerActive;
    h.update(long_side());
    assert(h.screen_==Screen::Clock && h.countdown_.state()==State::Running);
    h.screen_=Screen::TimerActive; h.timer_selection_=0;
    h.update(m5());
    assert(h.countdown_.state()==State::Paused);
    h.update(side()); h.update(side());
    assert(h.timer_selection_==2);
    h.update(m5());
    assert(h.screen_==Screen::Clock && h.countdown_.state()==State::Paused);
    h.screen_=Screen::TimerActive; h.timer_selection_=0;
    h.update(m5()); // Resume.
    assert(h.countdown_.state()==State::Running);
    h.update(side()); h.update(m5()); // RESETUJ still resets.
    assert(h.countdown_.resets==1 && h.countdown_.state()==State::Idle);
    assert(h.screen_==Screen::TimerSetup);
    // Previous Idle state still exits active timer into setup.
    h.screen_=Screen::TimerActive;
    h.update(m5());
    assert(h.screen_==Screen::TimerSetup);
    return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / "r5_navigation.cpp"
    executable = Path(tmp) / "r5_navigation"
    source.write_text(host, encoding="utf-8")
    subprocess.run([
        "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        str(source), "-o", str(executable),
    ], check=True)
    subprocess.run([str(executable)], check=True)

print("Clock R5 measured font fit, hints, production controls and background timer PASS")
