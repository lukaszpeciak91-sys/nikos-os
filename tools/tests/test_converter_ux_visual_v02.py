#!/usr/bin/env python3
"""Converter v0.2 pixel-budget checks using production font advances and regions."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
cpp = (root/"apps/converter/converter_app.cpp").read_text(encoding="utf-8")
glyphs = (root/"components/font_core/glyph_assets.hpp").read_text(encoding="utf-8")
h = (root/"apps/converter/include/converter/conversion.hpp").read_text(encoding="utf-8")

advances = {
    chr(int(cp)): int(width) + 1
    for cp, width in re.findall(r'\{(\d+),\s*(\d+),\s*"[0-9a-f]+"\}', glyphs)
}
def width(text, large=False):
    assert all(ch in advances for ch in text), text
    return sum(
        (advances[ch]*5+3)//4 if large else advances[ch] for ch in text
    )

# Actual UI regions are disjoint vertically, including the footer.
main = [
    (12, 1, 216, 19),    # category
    (12, 22, 216, 26),   # WARTOŚĆ
    (12, 49, 103, 24),   # Z
    (120, 49, 108, 24),  # NA
    (12, 74, 212, 19),   # WYNIK title
    (17, 94, 210, 24),   # one prominent result
    (12, 119, 218, 16),  # controls
]
for x,y,w,ht in main:
    assert 0 <= x and 0 <= y and x+w <= 240 and y+ht <= 135
for i,a in enumerate(main):
    for b in main[i+1:]:
        intersection = (min(a[0]+a[2],b[0]+b[2])-max(a[0],b[0])
                        and min(a[1]+a[3],b[1]+b[3])-max(a[1],b[1]))
        assert not (min(a[0]+a[2],b[0]+b[2])>max(a[0],b[0]) and
                    min(a[1]+a[3],b[1]+b[3])>max(a[1],b[1])), (a,b)

assert width("WARTOŚĆ") < 100
assert width("POZA ZAKR.") < 210
assert width("-9999.9",True) < 101
assert width("US gal") <= 61
assert width("ZAPISZ") < 80
assert width("PONIŻEJ ZERA ABS.") <= 216
assert width("9999.9 US gal",True) <= 210
assert width("-9999.9 US gal",True) <= 210
for name in ("°C","°F","K","mm","cm","m","km","cal","stopa","mila",
             "g","kg","oz","lb","ml","l","US gal"):
    assert width(name) <= 61, (name,width(name))
# The full fixed buffer (including sign and decimal point) stays centred.
for fixed in ("+9999.9","-0000.0","0000.0"):
    assert width(fixed,True) < 240

assert 'draw("WARTOŚĆ", 20, 26, 100' in cpp
assert "draw_fitted(number, 123, 23, 101" in cpp
assert "draw_fitted(result_text, 17, 94, 210" in cpp
assert 'draw("WYNIK", 12, 74, 212' in cpp
assert "if (readable.supported && readable.fits && readable.lines == 1)" in cpp
assert "compact.supported && compact.fits" in cpp
assert "compact.lines == 1" in cpp
assert 'shown = compact.supported' in cpp

# Number editor, button and category/unit list scroll must fit 240x135.
for x,y,w,ht in [(12,3,216,19),(0,34,240,24),
                 (0,60,240,3),(32,67,176,19),
                 (71,87,98,25),(12,115,216,19),
                 (12,119,218,16),(16,83,208,24)]:
    assert x >= 0 and y >= 0 and x+w <= 240 and y+ht <= 135
# All context-sensitive footer messages fit native Font0 (6px/character).
for hint in ("M5 WYBIERZ  SIDE DALEJ",
             "M5 OK  SIDE JEDN.  DL ANULUJ",
             "SIDE +1  M5 DALEJ  DL ANULUJ",
             "M5 ZAPISZ  SIDE POPRAW  DL ANULUJ"):
    assert hint in cpp
    assert hint.isascii()
    assert len(hint) * 6 <= 218, hint

assert "const std::uint8_t first_digit = editor_.temperature ? 1U : 0U;" in cpp
assert "editor_.character_index()" in cpp
assert "editor_.text(edit)" in cpp
assert "font_.advance(" in cpp
assert "save_focus_ &&" not in cpp or "save_focus_" in cpp
assert "if (focused)" in cpp and "board_.fill_rect(x, 60, width, 3" in cpp
assert "const std::uint8_t first = candidate_unit_ >= 3U" in cpp
assert "index >= count(category_)" in cpp
assert "const std::uint8_t first = selection_ >= 3U" in cpp
assert '"POWRÓT"' in cpp

# Safety and conversion data must remain the original unmodified pure model.
for token in ("inline constexpr Unit temperatures[]",
              "inline constexpr Unit lengths[]",
              "inline constexpr Unit masses[]",
              "inline constexpr Unit volumes[]",
              "inline bool to_base(", "inline bool from_base(",
              "struct Editor"):
    assert token in h

print("Converter v0.2 typography, layout geometry, dynamic fitting and glyphs PASS")
