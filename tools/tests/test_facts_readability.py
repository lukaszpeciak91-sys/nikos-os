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

# Match the actual 5:4 proportional readable PRE-RENDERED advances.
raster_readable = {c: (width * 5 + 3) // 4 for c, width in raster.items()}

# All nine Facts menu labels must fit the 224px viewport on one line.
facts_source = (ROOT / "apps/facts/facts_app.cpp").read_text(encoding="utf-8")
menu_section = facts_source.split("constexpr const char* kMainLabels[9] = {", 1)[1].split("};", 1)[0]
menu_labels = re.findall(r'"([^"]+)"', menu_section)
assert len(menu_labels) == 9, menu_labels
assert menu_labels[-2:] == ["QUIZ PRAWDA/FAŁSZ", "POWRÓT"]
assert "range_length(kMainLabels[index]), 224, 1" in facts_source
for mode, widths in (("STANDARD", legacy), ("COMPOSED", composed),
                     ("PRE-RENDERED", raster_readable)):
    for label in menu_labels:
        assert count_lines(label, widths, 224) == 1, (mode, label)

for name, widths, line_height in (
    ("STANDARD", legacy, 24),
    ("COMPOSED", composed, 19),
    ("PRE-RENDERED", raster_readable, 24),
):
    max_pages = 0
    tested_titles = 0
    one_page = 0
    multiple_pages = 0
    for ident, title, body in facts:
        tested_titles += 1
        title_lines = count_lines(title, widths, 232)
        assert 1 <= title_lines <= 3, (name, ident, "reader title")
        list_lines = count_lines(title, widths, 224)
        assert 1 <= list_lines <= 3, (name, ident, "category title")
        assert list_lines * line_height + 4 <= 87, (name, ident, "list viewport")
        divider_y = 18 + title_lines * line_height
        body_capacity = max(1, (119 - (divider_y + 3)) // line_height)
        assert divider_y < 119 and body_capacity >= 1
        lines = count_lines(body, widths, 232)
        pages = (lines + body_capacity - 1) // body_capacity
        assert 1 <= pages <= 12, (name, ident, pages)
        if lines <= body_capacity:
            assert pages == 1
            one_page += 1
        else:
            multiple_pages += 1
        max_pages = max(max_pages, pages)
    assert tested_titles == 600
    for item in quiz:
        assert count_lines(item["statement"], widths, 226) <= 3 * 8, (
            name, item["id"], "question")
        assert count_lines(item["explanation"], widths, 226) <= 2 * 8, (
            name, item["id"], "feedback")
    print(f"{name}: 600 facts, 120 quiz items; "
          f"one-page={one_page}, multipage={multiple_pages}, max={max_pages}")
print("Facts v0.2 measured data validation PASS")
