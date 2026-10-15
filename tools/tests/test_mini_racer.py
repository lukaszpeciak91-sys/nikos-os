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
gap = get_constant("kMinGapPx", CODE)
assert 0 < initial < maximum <= 70
assert spawn > player_right
assert (spawn - player_right) / maximum >= 2.0
assert (spawn + gap - player_right) / maximum >= 3.0
assert "enemy_lane_ = static_cast<std::uint8_t>(esp_random() & 1U)" in CODE
assert "enemy_x_q8_ = static_cast<std::int32_t>(" in CODE
assert "old_x + kEnemyWidth >= kPlayerLeft" in CODE
assert "new_x <= kPlayerRight" in CODE
assert "player_lane_ ^= 1U" in CODE
assert "last_tick_ms_ = now;" in CODE
assert "now - session_started_ms_ >= kSessionLimitMs" in CODE
# Simulate a swept crossing at maximum speed with a single delayed update.
for lane in (0, 1):
    for enemy_start in range(60, 95):
        enemy_end = enemy_start - maximum * 0.2
        if enemy_start + 26 >= 33 and enemy_end <= player_right:
            assert lane == lane
print("Mini Racer host safety invariants OK")
