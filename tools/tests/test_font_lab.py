#!/usr/bin/env python3
"""Host-side static verification of temporary Font Lab assets and wiring."""
import base64
import re
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
asset = (ROOT / "apps/font_lab/include/font_lab/bitmap_assets.hpp").read_text(encoding="utf-8")
app = (ROOT / "apps/font_lab/font_lab_app.cpp").read_text(encoding="utf-8")
main = (ROOT / "main/app_main.cpp").read_text(encoding="utf-8")
launcher = (ROOT / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
images = re.findall(r'^"([A-Za-z0-9+/=]+)",?$', asset, re.MULTILINE)
assert len(images) == 6, len(images)
assert all(len(base64.b64decode(image, validate=True)) == 360 for image in images)
for sentence in (
    "Zażółć gęślą jaźń", "Ą Ć Ę Ł Ń Ó Ś Ź Ż",
    "ą ć ę ł ń ó ś ź ż", "Łódź leży w Polsce.",
    "Żółwie żyją długo.", "Kąt padania światła.",
):
    assert sentence in asset
for c in "ąćęłńóśźżĄĆĘŁŃÓŚŹŻ":
    assert hex(ord(c)).upper().replace("0X", "0x") in app or (
        "0x%04X" % ord(c)) in app, c
assert "font_lab.update(input)" in main
assert "font_lab.redraw()" in main
assert "resume_font_lab_after_communicator" in main
assert "Action::OpenFontLab" in launcher
print("Font Lab assets and integration static checks PASS")
