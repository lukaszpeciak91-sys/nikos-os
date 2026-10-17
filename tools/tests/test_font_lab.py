#!/usr/bin/env python3
"""Retained Font Lab comparison/source checks; app is no longer product-wired."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
app = (ROOT / "apps/font_lab/font_lab_app.cpp").read_text(encoding="utf-8")
main = (ROOT / "main/app_main.cpp").read_text(encoding="utf-8")
launcher = (ROOT / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
root_cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
main_cmake = (ROOT / "main/CMakeLists.txt").read_text(encoding="utf-8")
glyphs = (ROOT / "components/font_core/glyph_assets.hpp").read_text(encoding="utf-8")

assert 'kNames[] = {"STANDARD", "COMPOSED", "PRE-RENDERED"}' in app
assert "kPageCount = 6" in app
samples = (
    "Zażółć gęślą jaźń", "Ą Ć Ę Ł Ń Ó Ś Ź Ż",
    "ą ć ę ł ń ó ś ź ż", "Łódź leży w Polsce.",
    "Żółwie żyją długo.", "Kąt padania światła.",
)
assert all(text in app for text in samples)
supported = set(range(32, 127))
supported.update(map(int, re.findall(r'\{(\d+),\s*\d+,\s*"[0-9a-f]+"\}', glyphs)))
for sample in samples:
    assert all(ord(ch) in supported for ch in sample), sample

# The retired diagnostic source stays useful for tests, not the user-facing app.
assert "font_lab" not in root_cmake + main_cmake + main
assert "OpenFontLab" not in launcher
assert "Test fontu" not in launcher and "TEST FONTU" not in launcher
assert "resume_font_lab_after_communicator" not in main
print("Retained Font Lab assets and product removal checks PASS")
