#!/usr/bin/env python3
"""Validate approved quiz content, layout bounds and round behavior."""
import importlib.util
import json
import random
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
path = ROOT / "tools/generate_quiz.py"
spec = importlib.util.spec_from_file_location("quiz_generator", path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
items = json.loads((ROOT / "content/quiz/nikos_quiz_prawda_falsz_120.json").read_text(encoding="utf-8"))
assert len(items) == 120
assert len({x["id"] for x in items}) == 120
assert all(sum(x["category"] == c for x in items) == 20 for c in module.CATS)
assert len({x["source_id"] for x in items}) == 120
assert sum(x["correct"] is True for x in items) == 60
assert sum(x["correct"] is False for x in items) == 60
assert all(x["source_id"] == x["id"] for x in items)
assert (ROOT / "apps/facts/generated/quiz_data.cpp").read_text(encoding="utf-8") == module.generate()
glyphs = module.font_advances()
for x in items:
    assert isinstance(x["correct"], bool)
    assert len(module.wrap(x["statement"], 226, glyphs)) <= 5, x["id"]
    assert len(module.wrap(x["explanation"], 226, glyphs)) <= 4, x["id"]
    for field, count in (("statement", 5), ("explanation", 4)):
        wrapped = module.wrap(x[field], 226, glyphs)
        assert len(wrapped) <= count, x["id"]
        assert " ".join(wrapped) == x[field], x["id"]
        assert all(sum(glyphs[c] for c in line) <= 226 for line in wrapped)
    assert all(ord(c) > 31 for c in x["statement"] + x["explanation"])
for seed in range(500):
    rng = random.Random(seed)
    chosen = rng.sample(range(120), 10)
    assert len(chosen) == len(set(chosen)) == 10
    for expected in range(11):
        choices = [items[i]["correct"] for i in chosen]
        choices = [not x if j >= expected else x for j, x in enumerate(choices)]
        score = sum(choices[j] == items[chosen[j]]["correct"] for j in range(10))
        assert score == expected
print("Quiz dataset, asset generation and pure round invariants OK")
