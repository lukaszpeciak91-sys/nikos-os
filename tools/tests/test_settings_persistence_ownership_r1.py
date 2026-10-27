#!/usr/bin/env python3
"""R1 architecture guard: UI proposes; composition persists and applies."""
from pathlib import Path
r = Path(__file__).resolve().parents[2]
header=(r/"components/launcher/include/launcher/launcher.hpp").read_text(encoding="utf-8")
launcher=(r/"components/launcher/launcher.cpp").read_text(encoding="utf-8")
app=(r/"main/app_main.cpp").read_text(encoding="utf-8")
architecture=(r/"docs/project/architecture.md").read_text(encoding="utf-8")
decisions=(r/"docs/project/decisions.md").read_text(encoding="utf-8")

assert '#include "storage/ui_preferences_store.hpp"' not in header
assert "storage::UiPreferencesStore" not in header
assert "ui_preferences_store_" not in launcher
for token in ("save_theme(", "save_orientation(", "save_brightness(", "save_signal_sound("):
    assert token not in launcher, f"Launcher still accesses NVS: {token}"
    assert "ui_preferences_store."+token in app, f"Composition does not persist {token}"
for change in ("ThemeChanged", "OrientationChanged", "BrightnessChanged",
               "SignalSoundChanged"):
    assert change in header and change in launcher and change in app
assert "complete_preference_change(" in header
assert "complete_preference_change(" in launcher
assert "complete_preference_change(" in app
assert "settings_save_failed_ = false;" in launcher
assert "settings::State" in architecture
assert "storage::UiPreferencesStore" in architecture
assert "composition" in architecture.lower()
assert "Settings Persistence v0.1" in decisions
print("Settings R1 architecture ownership and policy PASS")
