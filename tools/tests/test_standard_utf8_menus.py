#!/usr/bin/env python3
"""Legacy STANDARD regression replaced by fixed PRE-RENDERED menu guards."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
launcher = (root / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
main = (root / "main/app_main.cpp").read_text(encoding="utf-8")
for label in ("NARZĘDZIA", "WYŁĄCZ", "DŹWIĘK", "JASNOŚĆ", "POWRÓT",
              "AUTO WYŁĄCZ.", "ŁAGODNY", "GŁOŚNOŚĆ"):
    assert f'"{label}"' in launcher, label

helper = launcher.split("void Launcher::draw_pilot_text(", 1)[1].split(
    "void Launcher::render_sound_menu()", 1)[0]
assert "font_core::Mode::PreRendered" in helper
assert "font_core::Size::Readable" in helper
assert "font_core::Mode::Standard" not in helper
assert "FontSelector" not in launcher
assert "font_mode" not in launcher

# UTF-8 metadata must not reach byte-oriented Font0 unconverted.
assert "ctx.board->draw_polish_ui_text_region(" in main
assert "ctx.charging && ctx.charging->active" in main
print("Fixed PRE-RENDERED Polish menu regression checks PASS")
