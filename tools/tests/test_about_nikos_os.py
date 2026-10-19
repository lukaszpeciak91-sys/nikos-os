#!/usr/bin/env python3
"""About NIKOS OS is an informational screen, with no settings mutations."""
from pathlib import Path

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
assert "screen_ = Screen::About;" in settings_input

about_input = launcher.split("if (screen_ == Screen::About) {", 1)[1].split(
    "if (screen_ == Screen::SoundMenu) {", 1
)[0]
for event in ("input.primary_short", "input.secondary_short", "input.secondary_long"):
    assert event in about_input
assert "screen_ = Screen::Settings;" in about_input
assert "return Action::None;" in about_input
assert "settings_." not in about_input

about_render = launcher.split("void Launcher::render_about()", 1)[1].split(
    "void Launcher::draw_pilot_text(", 1
)[0]
for text in (
    '"NIKOS OS"',
    '"Jedyny taki system na świecie."',
    '"Stworzony specjalnie dla Nikodema."',
    '"2026"',
    '"Wszystkie prawa zastrzeżone"',
):
    assert text in about_render, text
assert "case Screen::About:" in launcher
assert "render_about();" in launcher
assert "        About," in header
assert "    void render_about();" in header
print("About NIKOS OS source contracts PASS")
