#!/usr/bin/env python3
"""Regression checks for the fixed PRE-RENDERED product typography baseline."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
def src(path):
    return (ROOT / path).read_text(encoding="utf-8")

launcher = src("components/launcher/launcher.cpp")
header = src("components/launcher/include/launcher/launcher.hpp")
main = src("main/app_main.cpp")
settings = src("components/settings/include/settings/settings.hpp")
cmake = src("CMakeLists.txt") + src("main/CMakeLists.txt")
facts = src("apps/facts/facts_app.cpp")
quiz = src("apps/facts/quiz.cpp")
glyphs = src("components/font_core/glyph_assets.hpp")

for term in ("FontSelector", "font_selection_", "OpenFontLab", "Test fontu",
             "font_mode", "render_font_selector"):
    assert term not in launcher + header, term
for term in ("FontLab", "font_lab", "resume_font_lab_after_communicator",
             "font_mode"):
    assert term not in main + cmake, term
assert "font_mode" not in settings
assert "settings_.font_mode" not in facts + quiz
assert "renderer.select(nikos::font_core::Mode::PreRendered)" in main
for name in ("facts", "quiz"):
    assert "renderer_.select(font_core::Mode::PreRendered);" in (
        facts if name == "facts" else quiz)

# Flashlight/Communicator/Charging renderings must be excluded from delegation.
assert "*ctx.active_screen == RuntimeState::Communicator" in main
assert "*ctx.active_screen == RuntimeState::Flashlight" in main
assert "ctx.charging && ctx.charging->active" in main

def labels(section):
    return re.findall(r'"([^"]+)"', section)

start = launcher.split("constexpr std::array<Entry, 6> kEntries =", 1)[1].split("};", 1)[0]
main_labels = labels(start)
assert main_labels == ["KOMUNIKATOR", "NARZĘDZIA", "ROZRYWKA",
                       "ZEGAR", "USTAWIENIA", "WYŁĄCZ"]
for heading in ("NARZĘDZIA", "ROZRYWKA", "DŹWIĘK", "GŁOŚNOŚĆ"):
    assert f'"{heading}"' in launcher
assert '"POWRÓT"' in launcher and '"ŁAGODNY"' in launcher

# Bitmap assets include Polish glyphs as scalar Unicode codepoints.
supported = set(range(32, 127))
supported.update(int(cp) for cp in re.findall(
    r'\{(\d+),\s*\d+,\s*"[0-9a-f]+"\}', glyphs))
for label in main_labels + ["POWRÓT", "GŁOŚNOŚĆ", "ŁAGODNY",
                              "JASNOŚĆ", "AUTO WYŁĄCZ.", "ŚREDNIA"]:
    assert label == label.upper(), label
    assert all(ord(ch) in supported for ch in label), label

for path in (
    "components/launcher/launcher.cpp",
    "apps/power_diag_app/power_diag_app.cpp",
    "apps/sound_meter/sound_meter_app.cpp",
    "apps/snake/snake_app.cpp",
    "apps/mini_racer/mini_racer_app.cpp",
):
    for label in re.findall(r'"([^"\\n]*)"', src(path)):
        # All visible UTF-8 strings in supported normal UI components must
        # have glyphs in the single shared raster asset set.
        if any(ord(ch) > 127 for ch in label):
            assert all(ord(ch) in supported for ch in label), (path, label)

# Preserved adjustable font renderer is still shared and bounded.
core = src("components/font_core/font_core.cpp")
assert "Mode::PreRendered" in core
assert "kGlyphAssets" in core
print("Typography consolidation source and glyph checks PASS")
