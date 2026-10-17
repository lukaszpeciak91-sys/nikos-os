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
print("Settings/Clock and Mini Racer pause layout guards PASS")
