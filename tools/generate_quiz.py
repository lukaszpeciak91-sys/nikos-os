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

def wrap(text, width):
    lines = [""]
    for word in text.split():
        if len(lines[-1]) + len(word) + (1 if lines[-1] else 0) <= width:
            lines[-1] += (" " if lines[-1] else "") + word
        else:
            lines.append(word)
    return lines

def generate():
    data = json.loads(SOURCE.read_text(encoding="utf-8"))
    if not isinstance(data, list) or len(data) != 120:
        raise ValueError("Expected exactly 120 records")
    seen = set()
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
        if len(wrap(r["statement"], 24)) > 5:
            raise ValueError(f"{ident}: question does not fit five lines")
        if len(wrap(r["explanation"], 24)) > 4:
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
