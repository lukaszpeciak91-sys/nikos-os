#!/usr/bin/env python3
"""Regression guards for STANDARD Unicode Launcher typography."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
source = (root / "components/launcher/launcher.cpp").read_text(encoding="utf-8")

assert '{"Narzędzia"}' in source
assert '{"Wyłącz"}' in source
assert '"NARZĘDZIA"' in source
for label in ("Dźwięk", "Jasność", "Powrót", "AUTO WYŁĄCZ."):
    assert f'"{label}"' in source, label

helper = source.split("void Launcher::draw_pilot_text(", 1)[1].split(
    "void Launcher::render_font_selector()", 1)[0]
assert "board_.draw_polish_ui_text_region(" in helper
assert "const std::uint8_t scale = standard_size;" in helper
assert "height >= 24 ? 2U : 1U" not in helper

for name, ending in (
    ("render_main", "render_tools"),
    ("render_tools", "render_entertainment"),
    ("render_entertainment", "render_clock"),
    ("render_settings", "draw_pilot_text"),
):
    block = source.split(f"void Launcher::{name}()", 1)[1].split(
        f"void Launcher::{ending}(", 1)[0]
    assert "font_core::Mode::Composed" in block, name
    assert "25" in block, name
assert "draw_pilot_text(\n        14,\n        experimental_font ? 4 : 8," in source
print("STANDARD UTF-8 menu typography source regression checks PASS")
