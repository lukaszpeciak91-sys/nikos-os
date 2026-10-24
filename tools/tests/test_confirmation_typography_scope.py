#!/usr/bin/env python3
"""Scope regression for typography consolidation confirmation-only follow-up."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
launcher = (root / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
main = (root / "main/app_main.cpp").read_text(encoding="utf-8")
assets = (root / "components/font_core/glyph_assets.hpp").read_text(encoding="utf-8")
supported = {chr(int(cp)) for cp in re.findall(r'\{(\d+),\s*\d+,\s*"[0-9a-f]+"\}', assets)}
supported.update(chr(n) for n in range(32, 127))
for function, title, next_name in (
    ("render_enable_communicator", "WŁĄCZYĆ", "render_active_communicator"),
    ("render_disable_communicator", "WYŁĄCZYĆ", "render_shutdown_confirm"),
    ("render_shutdown_confirm", "WYŁĄCZYĆ", "render_communicator_delivery_status"),
):
    block = launcher.split(f"void Launcher::{function}()", 1)[1].split(
        f"void Launcher::{next_name}()", 1)[0]
    assert f'draw_pilot_text(18, 12, 204, 25, "{title}"' in block, function
    assert "draw_pilot_text(18, 39, 204, 25," in block
    assert "draw_pilot_text(32, y, 176, 25, kChoices[index], 2," in block
    assert '"M5 WYBIERZ | BOCZNY DALEJ", 1' in block
    assert "67 + index * 26" in block
    assert 12 + 25 <= 39 and 39 + 25 <= 67
    assert 67 + 26 + 25 <= 121
    for label in re.findall(r'"([^"]+)"', block):
        if label in ("WŁĄCZYĆ", "WYŁĄCZYĆ", "KOMUNIKATOR?",
                     "NIKOŚ OS?", "TAK", "NIE"):
            assert all(ch in supported for ch in label), label

active = launcher.split("void Launcher::render_active_communicator()", 1)[1].split(
    "void Launcher::render_disable_communicator()", 1)[0]
assert "draw_pilot_text(" not in active
assert 'kChoices[2] = {' in active
assert "if (size <= 1 || h < 19) return false;" in main
assert "*ctx.active_screen == RuntimeState::Communicator" in main
assert "*ctx.active_screen == RuntimeState::Flashlight" in main
assert "ctx.charging && ctx.charging->active" in main
assert "ctx.launcher->showing_communicator_ui()" in main

# Shutdown confirmation defaults to TAK; other confirmations are unchanged.
shutdown_entry = re.search(
    r"case 5:\s*confirmation_selection_ = (\d+);\s*"
    r"screen_ = Screen::ShutdownConfirm;", launcher
)
assert shutdown_entry and shutdown_entry.group(1) == "0"
shutdown_controls = launcher.split(
    "if (screen_ == Screen::ShutdownConfirm) {", 1
)[1].split("void Launcher::update_battery_sample(", 1)[0]
assert "confirmation_selection_ ^= 1U;" in shutdown_controls
assert "if (confirmation_selection_ == 0U)" in shutdown_controls
assert "return Action::ShutdownRequested;" in shutdown_controls
assert "if (input.secondary_long)" in shutdown_controls
assert "screen_ = Screen::Main;" in shutdown_controls
assert "confirmation_selection_ = 1U;\n                screen_ = Screen::ClockDateConfirm;" in launcher
assert "confirmation_selection_ = 1;\n            screen_ = Screen::DisableCommunicator;" in launcher

print("Confirmation-only typography bounds and glyph checks PASS")
