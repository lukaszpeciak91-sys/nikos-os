#!/usr/bin/env python3
"""Focused host invariants for Mini Racer's bounded obstacle policy."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CODE = (ROOT / "apps/mini_racer/mini_racer_app.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "apps/mini_racer/include/mini_racer/mini_racer_app.hpp").read_text(encoding="utf-8")

def get_constant(name, source):
    match = re.search(r"\b" + name + r"\s*=\s*(\d+)", source)
    if match is None:
        raise AssertionError(name)
    return int(match.group(1))

initial = get_constant("kInitialSpeedQ8", CODE) // 256
maximum = get_constant("kMaxSpeedQ8", CODE) // 256
spawn = get_constant("kSpawnX", HEADER)
player_right = get_constant("kPlayerRight", HEADER)
spacing = get_constant("kEnemySpacing", HEADER)
count = get_constant("kEnemyCount", HEADER)
assert 0 < initial < maximum <= 70
assert count == 3
assert spawn > player_right
assert (spawn - player_right) / maximum >= 2.0
# Each new hazard is at least 1.5 seconds behind the previous one.
assert spacing / maximum >= 1.5
# Cars cannot overlap horizontally across lanes, and one lane stays free.
assert spacing > get_constant("kEnemyWidth", HEADER)
assert "rightmost + kEnemySpacing * 256" in CODE
assert "rightmost_lane ^ 1U" in CODE
assert "first_lane ^ (i & 1U)" in CODE
assert "old_x + kEnemyWidth >= kPlayerLeft" in CODE
assert "new_x <= kPlayerRight" in CODE
assert "player_lane_ ^= 1U" in CODE
assert "last_tick_ms_ = now;" in CODE
assert "now - session_started_ms_ >= kSessionLimitMs" in CODE
assert "render_playing(false)" in CODE
assert "if (full) board_.fill_rect(0, 0, 240, 135" in CODE
assert "y + 18, 6, 4" in CODE # two rear wheels
# Test swept horizontal overlap for both lanes, including maximal steps.
for obstacle_lane in (0, 1):
    for player_lane in (0, 1):
        for old_x in range(-32, 241):
            new_x = old_x - maximum * 0.2
            overlap = (old_x + 26 >= 33) and (new_x <= player_right)
            detection = obstacle_lane == player_lane and overlap
            if obstacle_lane != player_lane:
                assert not detection
            elif old_x >= player_right and new_x <= player_right:
                assert detection
# No round may exceed the wall-clock session cap, including pauses.
assert get_constant("kSessionLimitMs", HEADER) == 600000
print("Mini Racer host obstacle, collision and session invariants OK")
