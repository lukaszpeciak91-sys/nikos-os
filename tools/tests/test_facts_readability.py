#!/usr/bin/env python3
"""Exhaustive screen-width and page-capacity checks for all Facts and Quiz data."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FONT = (ROOT / "components/board/polish_ui_font.cpp").read_text(encoding="utf-8")
GLYPHS = (ROOT / "components/font_core/glyph_assets.hpp").read_text(encoding="utf-8")
glyph_body = FONT.split("const lgfx::GFXglyph kPolishUiGlyphs[] PROGMEM = {", 1)[1].split("};", 1)[0]
range_body = FONT.split("const lgfx::EncodeRange kPolishUiRanges[] PROGMEM = {", 1)[1].split("};", 1)[0]
advances = [int(x) for x in re.findall(
    r"\{\s*\d+,\s*\d+,\s*\d+,\s*(\d+),", glyph_body)]
legacy = {}
for lower, upper, base in re.findall(
    r"\{\s*0x([0-9A-F]+),\s*0x([0-9A-F]+),\s*(\d+)\s*\}", range_body
):
    first, last, index = int(lower, 16), int(upper, 16), int(base)
    for cp in range(first, last + 1):
        legacy[chr(cp)] = advances[index + cp - first] * 2

raster = {
    chr(int(cp)): int(advance) + 1
    for cp, advance in re.findall(r'\{(\d+), (\d+), "[0-9a-f]+"\}', GLYPHS)
}
composed_map = dict(zip(
    "ĄąĆćĘęŁłŃńÓóŚśŹźŻż",
    "AaCcEeLlNnOoSsZzZz",
))
composed = {ch: raster[ch] for ch in raster}
for target, base in composed_map.items():
    composed[target] = raster[base]

def count_lines(text, widths, max_width):
    lines = 1
    line = ""
    for word in text.split():
        candidate = (line + " " + word) if line else word
        if any(ch not in widths for ch in candidate):
            raise AssertionError(f"Unsupported glyph in {candidate!r}")
        if sum(widths[c] for c in candidate) <= max_width:
            line = candidate
        else:
            if not line or sum(widths[c] for c in word) > max_width:
                raise AssertionError(f"Unbreakable word: {word!r}")
            lines += 1
            line = word
    return lines

facts = []
for category in ("space", "animals", "technology", "human", "history", "world"):
    text = (ROOT / f"apps/facts/generated/facts_data_{category}.cpp").read_text(
        encoding="utf-8")
    records = re.findall(
        r'\{\s*"([^"]+)",\s*"((?:\\"|[^"])*)",\s*"((?:\\"|[^"])*)"\s*\}', text
    )
    assert len(records) == 100, (category, len(records))
    facts.extend(records)
assert len(facts) == 600
assert len({r[0] for r in facts}) == 600

quiz = json.loads((ROOT / "content/quiz/nikos_quiz_prawda_falsz_120.json")
                  .read_text(encoding="utf-8"))
assert len(quiz) == 120

for name, widths in (("STANDARD", legacy), ("COMPOSED", composed),
                     ("PRE-RENDERED", raster)):
    max_pages = 0
    for ident, title, body in facts:
        assert count_lines(title, widths, 232) <= 2, (name, ident, "title")
        segments = re.split(r'(?<=[.!?]) +', body)
        pages = sum((count_lines(segment, widths, 232) + 1) // 2
                    for segment in segments)
        assert 1 <= pages <= 12, (name, ident, pages)
        max_pages = max(max_pages, pages)
    for item in quiz:
        assert count_lines(item["statement"], widths, 226) <= 3 * 8, (
            name, item["id"], "question")
        assert count_lines(item["explanation"], widths, 226) <= 2 * 8, (
            name, item["id"], "feedback")
    print(f"{name}: all 600 facts and 120 quiz questions fit; max pages = {max_pages}")

print("Facts readability exhaustive data test PASS")
