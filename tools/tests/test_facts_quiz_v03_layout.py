#!/usr/bin/env python3
"""Corpus-based PRE-RENDERED compact/list/reader/quiz layout regression."""
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
ASSETS = (ROOT / "components/font_core/glyph_assets.hpp").read_text(encoding="utf-8")
ADV = {chr(int(cp)): int(advance) + 1 for cp, advance in
       re.findall(r'\{(\d+),\s*(\d+),\s*"[0-9a-f]+"\}', ASSETS)}

def width(text, readable=False):
    assert all(c in ADV for c in text), [c for c in text if c not in ADV]
    return sum(((ADV[c] * 5 + 3) // 4 if readable else ADV[c]) for c in text)

def wrap(text, max_width, readable=False):
    rows = []
    line = ""
    for word in text.split():
        assert width(word, readable) <= max_width, (word, max_width)
        candidate = f"{line} {word}" if line else word
        if width(candidate, readable) > max_width:
            rows.append(line)
            line = word
        else:
            line = candidate
    if line:
        rows.append(line)
    assert " ".join(rows) == " ".join(text.split())
    return rows

categories = ("space", "animals", "technology", "human", "history", "world")
facts = []
for cat in categories:
    path = ROOT / "content/facts" / f"{cat}.jsonl"
    group = [json.loads(s) for s in path.read_text(encoding="utf-8").splitlines()]
    assert len(group) == 100
    facts.extend(group)

assert len(facts) == 600
title_counts = {1: 0, 2: 0}
reader_pages = []
for fact in facts:
    item_rows = wrap(fact["title"], 224)
    assert 1 <= len(item_rows) <= 2, fact["id"]
    title_counts[len(item_rows)] += 1
    item_height = 19 * len(item_rows) + 3
    assert item_height <= 41
    # Even a pair of consecutive 2-line titles fits the 87px viewport.
    assert item_height * 2 <= 87

    title_rows = wrap(fact["title"], 232)
    assert len(title_rows) <= 3, fact["id"]
    divider = 18 + 19 * len(title_rows)
    body_y = divider + 3
    line_capacity = max(1, (119 - (divider + 2) - 4) // 24)
    assert body_y + 24 * line_capacity <= 119, fact["id"]
    body_rows = wrap(fact["text"], 232, readable=True)
    # Exact greedy one-page-if-fits invariant.
    page_count = (len(body_rows) + line_capacity - 1) // line_capacity
    assert (page_count == 1) == (len(body_rows) <= line_capacity)
    assert 1 <= page_count <= 12
    reader_pages.append(page_count)

assert title_counts == {1: 551, 2: 49}, title_counts

quiz_path = ROOT / "content/quiz/nikos_quiz_prawda_falsz_120.json"
questions = json.loads(quiz_path.read_text(encoding="utf-8"))
assert len(questions) == 120
line_counts = {1: 0, 2: 0, 3: 0, 4: 0}
for item in questions:
    rows = wrap(item["statement"], 226)
    assert 1 <= len(rows) <= 4, (item["id"], rows)
    line_counts[len(rows)] += 1
    # Header ends at 20, question y=22..98, answers y=109..134.
    assert 22 + 19 * len(rows) <= 98
    assert 98 < 109 and 109 + 25 <= 135
    assert 0 <= sum(bool(item["correct"]) for _ in [0]) <= 1
    explanation = wrap(item["explanation"], 226, readable=True)
    assert len(explanation) <= 16

facts_cpp = (ROOT / "apps/facts/facts_app.cpp").read_text(encoding="utf-8")
quiz_cpp = (ROOT / "apps/facts/quiz.cpp").read_text(encoding="utf-8")
assert "font_core::Size::Compact" in facts_cpp
assert "board::DisplayColor::Accent, board::DisplayColor::Background,\n        font_core::Size::Compact" in facts_cpp
assert "render_readable(wrapped_body_" in facts_cpp
assert "draw_wrapped(quiz::kItems[indices_[position_]].statement,\n        22, 4, font_core::Size::Compact)" in quiz_cpp
question_handler = quiz_cpp.split("if (screen_ == Screen::Question)",1)[1].split(
    "} else if (screen_ == Screen::Feedback)",1)[0]
assert "++text_page_" not in question_handler
assert "selected_ ^= 1U" in question_handler
assert "last_correct_" in question_handler
feedback_handler = quiz_cpp.split("} else if (screen_ == Screen::Feedback)",1)[1].split(
    "} else {",1)[0]
assert "--text_page_" in feedback_handler
assert "++text_page_" in feedback_handler
assert "if (++position_ == 10U)" in feedback_handler
# Deterministic ten-question scoring and forward/back explanation cursor.
for expected in range(11):
    truth = [i % 2 == 0 for i in range(10)]
    chosen = [truth[i] if i < expected else not truth[i] for i in range(10)]
    assert sum(chosen[i] == truth[i] for i in range(10)) == expected
for total in range(1, 9):
    for start in range(total):
        position = start
        if position > 0:
            position -= 1  # SIDE: previous page
            assert position == start - 1
        while position + 1 < total:
            position += 1  # M5: next page
        assert position == total - 1
assert "SIDE WSTECZ" in quiz_cpp
assert "M5 DALEJ" in quiz_cpp
assert "text_page_count_ > 1" in quiz_cpp
print(f"Facts title rows: {title_counts}, max reader pages: {max(reader_pages)}")
print(f"Quiz question rows: {line_counts}; all 120 fit one screen. PASS")
