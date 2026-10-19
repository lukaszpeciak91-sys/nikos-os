#!/usr/bin/env python3
"""Protect readable two-page About screen and Settings navigation."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
launcher = (root / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
header = (root / "components/launcher/include/launcher/launcher.hpp").read_text(encoding="utf-8")

settings_render = launcher.split("void Launcher::render_settings()", 1)[1].split(
    "void Launcher::render_about()", 1
)[0]
assert "kItems[7]" in settings_render
assert settings_render.index('"AUTO WYŁĄCZ."') < settings_render.index(
    '"O NIKOS OS"'
) < settings_render.index('"POWRÓT"')

settings_input = launcher.split("if (screen_ == Screen::Settings) {", 1)[1].split(
    "if (screen_ == Screen::About) {", 1
)[0]
assert "(settings_selection_ + 1U) % 7U" in settings_input
assert "settings_selection_ == 5U" in settings_input
assert "about_page_ = 0;" in settings_input
assert "screen_ = Screen::About;" in settings_input

about_input = launcher.split("if (screen_ == Screen::About) {", 1)[1].split(
    "if (screen_ == Screen::SoundMenu) {", 1
)[0]
for event in ("input.primary_short", "input.secondary_short", "input.secondary_long"):
    assert event in about_input
assert "about_page_ = 1U;" in about_input
assert "--about_page_;" in about_input
assert about_input.count("screen_ = Screen::Settings;") == 3
assert "return Action::None;" in about_input
assert "settings_." not in about_input

about_render = launcher.split("void Launcher::render_about()", 1)[1].split(
    "void Launcher::draw_pilot_text(", 1
)[0]
assert "font_core::Mode::PreRendered" in about_render
assert "font_core::Size::Readable" in about_render
assert "font_core::Size::Compact" not in about_render
assert "draw_polish_ui_text_region" not in about_render
assert "polish_ui_text_width" not in about_render
assert "about_page_ == 0U" in about_render
assert '"1/2"' in about_render and '"2/2"' in about_render
assert '"SIDE <"' in about_render and '"M5 >"' in about_render
assert "case Screen::About:" in launcher and "render_about();" in launcher
assert "        About," in header and "    void render_about();" in header
assert "    std::uint8_t about_page_ = 0;" in header

readable_strings = [
    "NIKOS OS",
    "Jedyny taki system",
    "na świecie.",
    "Stworzony specjalnie",
    "dla Nikodema.",
    "2026",
    "Wszystkie prawa",
    "zastrzeżone",
    "SIDE <",
    "M5 >",
]
for line in readable_strings:
    assert f'"{line}"' in about_render, line

# Match the actual glyph advance/scaling used by font_core::Size::Readable.
assets = (root / "components/font_core/glyph_assets.hpp").read_text(encoding="utf-8")
advances = {
    int(cp): int(advance)
    for cp, advance in re.findall(
        r'\{\s*(\d+)\s*,\s*(\d+)\s*,\s*"[0-9a-f]+"\s*\}',
        assets,
    )
}
assert len(advances) >= 95
for line in readable_strings:
    assert all(ord(ch) in advances for ch in line), line
    width = sum(((advances[ord(ch)] + 1) * 5 + 3) // 4 for ch in line)
    # 228px available for centered text; dedicated footer regions are narrower.
    limit = 92 if line == "SIDE <" else 60 if line == "M5 >" else 62 if line == "2026" else 228
    assert width <= limit, f"{line!r}: {width}px exceeds {limit}px"

assert "board_.fill_circle(95, 60, 10," in about_render
assert "font_renderer_.draw(\"2026\", {115, 48, 62, 24}" in about_render
print("Readable two-page About NIKOS OS contracts PASS")
