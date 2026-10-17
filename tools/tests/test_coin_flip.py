#!/usr/bin/env python3
"""Compile the production coin rules and verify deterministic state transitions."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("SKIP: no C++17 host compiler")
program = r"""
#include <cassert>
#include <cstdint>
#include "coin_flip/coin_rules.hpp"
using namespace nikos::coin_flip;
int main() {
    State s;
    s.begin(100);
    assert(s.phase == Phase::Idle);
    assert(s.toss(100, 0U));
    assert(s.result == Face::Eagle);
    assert(!s.toss(200, 1U)); // cannot overwrite a pending result
    assert(s.result == Face::Eagle);
    assert(!s.tick(149));
    assert(s.tick(175));
    assert(s.frame == 1);
    s.suspend();
    assert(!s.tick(100000));
    assert(s.phase == Phase::Flipping && s.result == Face::Eagle);
    s.resume(100000);
    assert(!s.tick(100010));
    assert(s.elapsed_ms == 85);
    assert(s.tick(100825));
    assert(s.phase == Phase::Result);
    assert(s.result == Face::Eagle);
    assert(!s.tick(101000)); // idle CPU stays quiet
    assert(s.toss(101001, 1U));
    assert(s.result == Face::Tails && s.phase == Phase::Flipping);
    assert(!s.toss(101002, 0U));
    assert(s.tick(101901));
    assert(s.phase == Phase::Result && s.result == Face::Tails);
    // Uniform random 32-bit words split into equal halves by their low bit.
    static_assert(from_random(0U) == Face::Eagle);
    static_assert(from_random(1U) == Face::Tails);
    static_assert(from_random(2U) == Face::Eagle);
    static_assert(from_random(3U) == Face::Tails);
    return 0;
}
"""
with tempfile.TemporaryDirectory() as temp:
    src = Path(temp) / "coin_rules_test.cpp"
    exe = Path(temp) / "coin_rules_test"
    src.write_text(program, encoding="utf-8")
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(root / "apps/coin_flip/include"),
                    str(src), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
app = (root / "apps/coin_flip/coin_flip_app.cpp").read_text(encoding="utf-8")
assert "if (!active_ || input.secondary_long)" in app
assert "if (input.primary_short && !animation_at_entry)" in app
assert "state_.toss(now, esp_random())" in app
assert "state_.phase == Phase::Result && state_.result == Face::Eagle" in app
assert 'state_.result == Face::Eagle ? "ORZEŁ!" : "RESZKA!"' in app
assert "vTaskDelay" not in app and "delay(" not in app
assert "M5: PONOWNIE" in app and "M5: RZUĆ" in app
print("Coin Flip deterministic C++17 state and app contract tests PASS")
