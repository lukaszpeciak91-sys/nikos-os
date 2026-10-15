#!/usr/bin/env python3
"""Deterministically validate and generate approved quiz firmware assets."""
import argparse
import json
from pathlib import Path
ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "content/quiz/nikos_quiz_prawda_falsz_120.json"
TARGET = ROOT / "apps/facts/generated/quiz_data.cpp"
CATS = ("space", "animals", "technology", "human", "history", "world")

def cpp(value):
    return json.dumps(value, ensure_ascii=False)

def font_advances():
    """Read exact xAdvance and Unicode range mapping from the firmware font."""
    import re
    source = (ROOT / "components/board/polish_ui_font.cpp").read_text(
        encoding="utf-8"
    )
    glyphs = source.split("const lgfx::GFXglyph kPolishUiGlyphs[] PROGMEM = {", 1)[1].split("};", 1)[0]
    ranges = source.split("const lgfx::EncodeRange kPolishUiRanges[] PROGMEM = {", 1)[1].split("};", 1)[0]
    advances = [
        int(x) for x in re.findall(
            r"\{\s*\d+,\s*\d+,\s*\d+,\s*(\d+),\s*-?\d+,\s*-?\d+\s*\}",
            glyphs,
        )
    ]
    mapping = {}
    for low, high, base in re.findall(
        r"\{\s*0x([0-9A-F]+),\s*0x([0-9A-F]+),\s*(\d+)\s*\}", ranges
    ):
        a, b, index = int(low, 16), int(high, 16), int(base)
        for cp in range(a, b + 1):
            mapping[chr(cp)] = advances[index + cp - a]
    return mapping


def wrap(text, width, glyphs=None):
    glyphs = glyphs if glyphs is not None else font_advances()
    lines = []
    line = ""
    for word in text.split():
        if any(char not in glyphs for char in word):
            raise ValueError(f"Unsupported glyph in: {word!r}")
        candidate = (line + " " + word) if line else word
        if sum(glyphs[char] for char in candidate) <= width:
            line = candidate
        else:
            if not line or sum(glyphs[char] for char in word) > width:
                raise ValueError(f"Word exceeds available glyph width: {word!r}")
            lines.append(line)
            line = word
    if line:
        lines.append(line)
    return lines


def generate():
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    if not isinstance(data, list) or len(data) != 120:
        raise ValueError("Expected exactly 120 records")
    seen = set()
    glyphs = font_advances()
    if sum(record['correct'] is True for record in data) != 60 or sum(record['correct'] is False for record in data) != 60:
        raise ValueError('Expected 60 true and 60 false answers')
    for cat in CATS:
        records = [r for r in data if r.get("category") == cat]
        if len(records) != 20:
            raise ValueError(f"{cat}: expected 20 items")
    if {r.get("category") for r in data} != set(CATS):
        raise ValueError("Unexpected category")
    for r in data:
        ident = r.get("id")
        if not isinstance(ident, str) or ident in seen:
            raise ValueError(f"Invalid/duplicate ID: {ident}")
        seen.add(ident)
        if any(not isinstance(r.get(k), str) or not r[k] for k in
               ("category", "source_id", "statement", "explanation")):
            raise ValueError(f"{ident}: invalid fields")
        if type(r.get("correct")) is not bool:
            raise ValueError(f"{ident}: expected boolean answer")
        if len(wrap(r["statement"], 226, glyphs)) > 5:
            raise ValueError(f"{ident}: question does not fit five lines")
        if len(wrap(r["explanation"], 226, glyphs)) > 4:
            raise ValueError(f"{ident}: explanation does not fit four lines")
        r["statement"].encode("utf-8", errors="strict")
        r["explanation"].encode("utf-8", errors="strict")
    rows = [
        "    {" + ", ".join((cpp(r["id"]), cpp(r["category"]),
        cpp(r["source_id"]), cpp(r["statement"]),
        "true" if r["correct"] else "false", cpp(r["explanation"]))) + "},"
        for r in data
    ]
    return '#include "facts/quiz_data.hpp"\nnamespace nikos::facts::quiz {\nconst Item kItems[kItemCount] = {\n' + "\n".join(rows) + '\n};\n}\n'

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    expected = generate()
    if args.check:
        if TARGET.read_text(encoding="utf-8") != expected:
            raise SystemExit("Stale quiz asset")
        print("Quiz asset validation OK: 120 questions")
    else:
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        TARGET.write_text(expected, encoding="utf-8", newline="\n")
