#!/usr/bin/env python3
"""Snake session cleanup: the central Entertainment Guard remains authoritative."""
from pathlib import Path

root = Path(__file__).resolve().parents[2]
snake = (root / "apps/snake/snake_app.cpp").read_text(encoding="utf-8")
header = (root / "apps/snake/include/snake/snake_app.hpp").read_text(encoding="utf-8")
main = (root / "main/app_main.cpp").read_text(encoding="utf-8")
guard = (root / "components/entertainment_guard/include/entertainment_guard/guard.hpp").read_text(encoding="utf-8")
coin = (root / "apps/coin_flip/coin_flip_app.cpp").read_text(encoding="utf-8")
racer = (root / "apps/mini_racer/mini_racer_app.cpp").read_text(encoding="utf-8")
defender = (root / "apps/space_defender/space_defender_app.cpp").read_text(encoding="utf-8")

assert "kSessionLimitMs" not in snake
assert "session_started_ms_" not in snake + header
assert "State::TimeLimit" not in snake
assert "render_time_limit" not in snake + header
assert "KONIEC SESJI" not in snake
assert "start_game(now);" in snake and "State::GameOver" in snake
assert "render_game_over();" in snake and "secondary_long" in snake
assert "suspend_for_entertainment_warning()" in snake
assert "resume_from_entertainment_warning()" in snake
assert "display_lifecycle_.note_visible_activity();" in snake
assert "kWindowMs=20U*60U*1000U" in guard
assert "kBreakMs=60U*60U*1000U" in guard
assert "if (entertainment_locked || reserve_entertainment)" in main
assert "snake.end();" in main
assert "resume_snake_after_communicator = false;" in main
assert "if (guard_entry && !entry_refused)" in main
assert "battery_guard.low_battery_reserve_active()" in main
assert "entertainment_guard.enter(" in main
assert "entertainment_guard.phase(" in main
assert "if (entertainment_warning_visible &&" in main
assert "snake.suspend_for_entertainment_warning();" in main
assert "snake.resume_from_entertainment_warning();" in main
assert "race::stage_speed(" in racer
assert "round_.begin(now);" in racer
assert "session_started_ms_" not in racer
assert "rules::kSessionMs" in defender
assert "session_started_ms_" in defender
assert "CoinFlipApp::update(" in coin
assert "CoinFlipApp::begin(" in coin
print("Snake/Guard central policy and game-limit isolation PASS")
