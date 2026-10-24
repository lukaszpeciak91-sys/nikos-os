#!/usr/bin/env python3
"""Approved VW Garage content, guard wiring, font and navigation regression."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
catalogue = (root / "apps/vw_garage/include/vw_garage/catalogue.hpp").read_text(encoding="utf-8")
main = (root / "main/app_main.cpp").read_text(encoding="utf-8")
launcher = (root / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
app = (root / "apps/vw_garage/vw_garage_app.cpp").read_text(encoding="utf-8")
entry_pattern = re.compile(
    r'\{\s*"([^"]+)",\s*"([^"]+)",\s*(\d{4}),\s*"([^"]+)",\s*'
    r'"((?:[^"\\]|\\.)*)"\s*\}', re.MULTILINE)
rows = [
    dict(id=m[1], name=m[2], year=int(m[3]), type=m[4],
         description=m[5])
    for m in entry_pattern.finditer(catalogue)
]
assert len(rows) == 25, len(rows)
assert len({r["id"] for r in rows}) == 25
assert [r["year"] for r in rows] == sorted(r["year"] for r in rows)
for row in rows:
    assert row["description"].count(r"\n") == 1
    assert row["description"].count(".") == 1
    assert row["description"].endswith(".")
    assert "http" not in row["description"]
    assert row["type"] in {"SEDAN", "COUPE", "VAN", "SUV", "HATCHBACK"}

# Digest of precisely the 25 approved editorial source records, including
# order, Polish Unicode, year, type and literal newline separators.
digest = hashlib.sha256(json.dumps(rows, ensure_ascii=False,
    separators=(",", ":")).encode("utf-8")).hexdigest()
assert digest == "921870aa6909e67efdecc01379b31b4d8b68ed4632341d87fc45e30d5b052b28", digest

assert 'Action::OpenVwGarage' in main and 'Action::OpenVwGarage' in launcher
assert 'RuntimeState::VwGarage' in main
assert '|| resume_vw_garage_after_communicator' in main
assert 'vw_garage.end();' in main and 'vw_garage.redraw();' in main
assert 'vw_garage.update(input)' in main
assert 'entertainment_guard.phase(loop_now_ms)' in main
assert 'launcher.take_entertainment_launch()' in main
assert 'low_battery_reserve_active()' in main
# Use the existing 1x Polish UI font for the requested accented labels;
# verify actual glyph advances against the unchanged 218 px footer bounds.
font = (root / "components/board/polish_ui_font.cpp").read_text(encoding="utf-8")
glyphs = font.split("const lgfx::GFXglyph kPolishUiGlyphs[] PROGMEM = {", 1)[1].split("};", 1)[0]
ranges = font.split("const lgfx::EncodeRange kPolishUiRanges[] PROGMEM = {", 1)[1].split("};", 1)[0]
advances = [int(match[0]) for match in re.findall(
    r"\{\s*\d+,\s*\d+,\s*\d+,\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)\s*\}", glyphs)]
widths = {}
for lo, hi, offset in re.findall(
        r"\{\s*0x([0-9A-F]+),\s*0x([0-9A-F]+),\s*(\d+)\s*\}", ranges):
    for cp in range(int(lo, 16), int(hi, 16) + 1):
        widths[chr(cp)] = advances[int(offset) + cp - int(lo, 16)]
for y, label in ((121, "M5 OTWÓRZ | SIDE DALEJ"),
                 (122, "M5 NASTĘPNY | SIDE WRÓĆ")):
    assert (f'board_.draw_polish_ui_text_region(12, {y}, 218, 12,\n'
            f'        "{label}", 1,') in app
    assert all(ch in widths for ch in label)
    assert sum(widths[ch] for ch in label) <= 218

for banned in ("http://", "https://", "esp_http_client", "wifi_start",
               "nvs_set", "malloc(", "new ", "std::vector", "std::string"):
    assert banned not in app and banned not in catalogue, banned

with tempfile.TemporaryDirectory() as tmp:
    binary = str(Path(tmp) / "test_vw_garage")
    subprocess.run([
        "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-I" + str(root / "tools/tests/font_core_stubs"),
        "-I" + str(root / "apps/vw_garage/include"),
        "-I" + str(root / "components/font_core/include"),
        str(root / "tools/tests/test_vw_garage.cpp"),
        str(root / "apps/vw_garage/vw_garage_app.cpp"),
        str(root / "components/font_core/font_core.cpp"),
        "-o", binary,
    ], check=True)
    subprocess.run([binary], check=True)
print("VW Garage approved editorial digest and host checks PASS")
