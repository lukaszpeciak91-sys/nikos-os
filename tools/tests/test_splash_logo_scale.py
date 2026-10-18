#!/usr/bin/env python3
"""Protect the approved splash-only OS scale and rendering sequence."""
from pathlib import Path

src = (Path(__file__).resolve().parents[2] /
       "components/launcher/launcher.cpp").read_text(encoding="utf-8")
logo = src.split("draw_sliced_logo(nikos::board::Board& board, bool show_os)", 1)[1].split(
    "}  // namespace", 1)[0]
splash = src.split("void Launcher::show_splash()", 1)[1].split(
    "void Launcher::begin(", 1)[0]

assert '"NIKOS",\n        4,\n        nikos::board::DisplayColor::PrimaryText' in logo
assert 'for (std::int16_t y : {58, 64, 70})' in logo
assert '''            169,
            80,
            64,
            38,
            "OS",
            4,
            nikos::board::DisplayColor::Accent''' in logo
assert logo.index("for (std::int16_t y :") < logo.index("if (show_os)")
assert splash.index("draw_sliced_logo(board_, false);") < splash.index(
    "draw_sliced_logo(board_, true);")
for text in ("kSignalFrameMs", "kSyncFrameMs", "kFormFrameMs"):
    assert text in splash
assert "750" not in splash  # final hold remains the owning boot code's responsibility
# Native Font0 at scale four is nominally 48x32 for two 6x8 glyph cells.
assert 169 + 48 <= 240 and 80 + 32 <= 135
assert 80 >= 34 + 4 * 8
print("Splash logo v0.2 geometry and sequence PASS")
