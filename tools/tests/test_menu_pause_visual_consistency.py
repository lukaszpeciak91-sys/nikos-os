#!/usr/bin/env python3
"""Geometry and scope regression for readable Settings/Clock rows and Racer pause."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
launcher = (root / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
racer = (root / "apps/mini_racer/mini_racer_app.cpp").read_text(encoding="utf-8")
space = (root / "apps/space_defender/space_defender_app.cpp").read_text(encoding="utf-8")
font = (root / "components/font_core/font_core.cpp").read_text(encoding="utf-8")

assert "return {24, 20, 1}" in font  # PreRendered Readable metrics
row_height, gap = 24, 3
for top, visible, footer_top in [(35, 3, 116), (31, 3, 113), (57, 2, 122)]:
    rows = [(top + i * (row_height + gap), top + i * (row_height + gap) + row_height)
            for i in range(visible)]
    assert all(0 <= y0 < y1 <= 135 for y0, y1 in rows)
    assert all(a[1] <= b[0] for a, b in zip(rows, rows[1:]))
    assert rows[-1][1] <= footer_top

assert "renderer.metrics().line_height" in launcher
assert "font_core::Mode::PreRendered" in launcher
assert "font_core::Size::Readable" in launcher
assert "kClockVisibleRows = 2" in launcher
assert "kClockMenuRowHeight = 27" in launcher
assert "clock_selection_ - kClockVisibleRows + 1U" in launcher
assert "first + slot < count" in launcher
for label in ("NIKOŚ", "ŚREDNIA", "ŁAGODNY", "GŁOŚNOŚĆ", "WYŁĄCZONE",
              "WZNÓW GRĘ", "WYJDŹ"):
    assert f'"{label}"' in (launcher + racer), label

pause = racer.split("if (!game_over) {", 1)[1].split("return;", 1)[0]
for text in ('"PAUZA"', '"WZNÓW GRĘ"', '"WYJDŹ"', '"SIDE ZMIANA   M5 OK"'):
    assert text in pause
for x, y, w, h in [(14, 10, 212, 26), (12, 41, 220, 17),
                   (18, 65, 208, 25), (18, 93, 208, 25),
                   (12, 125, 224, 10)]:
    assert 0 <= x and x + w <= 240 and 0 <= y and y + h <= 135
    assert f"({x}, {y}, {w}, {h}," in pause
assert "if (!game_over)" in racer
assert 'game_over ? "KONIEC GRY" : "PAUZA"' in racer
assert '"PAUZA"' in space

# All three sound selection handlers must take the same full-render route
# as initial screen entry; no legacy incremental raster can remain.
assert "redraw_sound_selection_rows" not in launcher
for screen, selection, count in (
    ("SoundMenu", "sound_menu_selection_", 3),
    ("SignalVolume", "signal_volume_selection_", 6),
    ("SignalSound", "signal_sound_selection_", 4),
):
    handler = launcher.split(f"if (screen_ == Screen::{screen}) {{", 1)[1]
    handler = handler.split("if (screen_ == Screen::", 1)[0]
    action = handler.split("if (input.secondary_short) {", 1)[1].split("return Action::None;", 1)[0]
    assert f"({selection} + 1U) % {count}U" in action, screen
    assert "render();" in action, screen
    assert "draw_text_region" not in action, screen

# The same viewport logic applies to all sound submenus. Check each
# possible selected index, scrolling boundary and wraparound to zero.
menu_calls = {
    "render_sound_menu": (3, "sound_menu_selection_", -1),
    "render_signal_volume": (6, "signal_volume_selection_",
                             "settings_.signal_volume_level"),
    "render_signal_sound": (4, "signal_sound_selection_",
                            "signal_sound_index(settings_.signal_sound)"),
}
for func, (count, state, indicator) in menu_calls.items():
    body = launcher.split(f"void Launcher::{func}()", 1)[1].split(
        "void Launcher::", 1)[0]
    assert "draw_readable_menu(board_, font_renderer_, kItems," in body
    assert f"{count}U, {state}, 35, 3," in body
    assert str(indicator) in body
    for selected in range(count):
        first = selected - 2 if selected >= 3 else 0
        visible = tuple(range(first, min(count, first + 3)))
        assert selected in visible
        assert len(visible) <= 3
        assert all(0 <= i < count for i in visible)
        next_selected = (selected + 1) % count
        next_first = next_selected - 2 if next_selected >= 3 else 0
        assert next_selected in range(next_first, min(count, next_first + 3))

# Full render clears stale rows before drawing the new viewport.
helper = launcher.split("void draw_readable_menu(", 1)[1].split(
    "void clear_logo_area(", 1)[0]
assert "renderer.metrics().line_height" in helper
assert "first + slot < count" in helper
for func in menu_calls:
    body = launcher.split(f"void Launcher::{func}()", 1)[1].split(
        "void Launcher::", 1)[0]
    assert "clear_shell(board_);" in body


# Use the actual generated glyph advances, not approximate character widths,
# for every dynamic Clock timer label supported by the product.
glyphs = (root / "components/font_core/glyph_assets.hpp").read_text(encoding="utf-8")
advances = {int(cp): int(advance) for cp, advance in
            re.findall(r'\\{(\\d+),\\s*(\\d+),\\s*"', glyphs)}
def readable_width(value):
    return sum(((advances[ord(char)] + 1) * 5 + 3) // 4 for char in value)

for label in ("MINUTNIK", "MINUTNIK KONIEC", "MINUTNIK 00:30",
              "MINUTNIK 15:00"):
    assert readable_width(label) <= 196, (label, readable_width(label))
assert "font_renderer_.draw(label, {22, y, 196, height}" in launcher

print("Settings/Clock, sound selection and Mini Racer pause layout guards PASS")
