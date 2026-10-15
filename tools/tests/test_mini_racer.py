#!/usr/bin/env python3
"""Compile and exercise Mini Racer's production traffic/state rules on host."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
header_dir = root / "apps/mini_racer/include"
compiler = shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("A C++17 host compiler is required")

source = r"""
#include <cassert>
#include <cstdint>
#include "mini_racer/traffic_rules.hpp"
using namespace nikos::mini_racer::rules;
int main() {
    Traffic traffic{};
    initialize(traffic, 255, 0);
    static_assert(kEnemyCount == 3, "must have three vehicles");
    assert(traffic.size() == 3);
    for (std::size_t i = 1; i < traffic.size(); ++i) {
        assert(traffic[i].x_q8 - traffic[i-1].x_q8 == kSpacingPx * 256);
        assert(traffic[i].lane != traffic[i-1].lane);
    }
    // A car fully beyond the left edge is placed after the rightmost.
    traffic[0].x_q8 = -30 * 256;
    assert(!advance(traffic, 70 * 256, 60, 0));
    assert(traffic[0].x_q8 == traffic[2].x_q8 + kSpacingPx * 256);
    assert(traffic[0].lane != traffic[2].lane);

    // A capped-speed swept collision cannot skip a matching-lane car.
    initialize(traffic, 255, 0);
    traffic[0].x_q8 = 60 * 256;
    assert(advance(traffic, 70 * 256, 60, 0));
    initialize(traffic, 255, 0);
    traffic[0].x_q8 = 60 * 256;
    assert(!advance(traffic, 70 * 256, 60, 1));

    // Repeated recycling never creates coincident opposite-lane barriers.
    initialize(traffic, 255, 0);
    for (int step = 0; step < 20000; ++step) {
        // Avoid collisions without changing the production update path.
        auto simulation = traffic;
        for (auto& e : simulation) e.lane = 2;
        assert(!advance(simulation, 70 * 256, 60, 0));
        traffic = simulation;
        // Restore a deterministic alternating two-lane assignment in x order.
        for (auto& e : traffic) e.lane = 0;
        for (std::size_t i = 0; i < traffic.size(); ++i)
            for (std::size_t j = i + 1; j < traffic.size(); ++j)
                assert(traffic[i].x_q8 != traffic[j].x_q8);
    }
    State state = State::Playing;
    state = pause(state);
    assert(state == State::Paused);
    assert(pause(state) == State::Paused);
    state = resume(state);
    assert(state == State::Playing);
    state = hit(state);
    assert(state == State::GameOver);
    assert(resume(state) == State::GameOver);
    assert(hit(State::Paused) == State::Paused);
}
"""
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / "mini_racer_host.cpp"
    exe = Path(tmp) / "mini_racer_host"
    cpp.write_text(source, encoding="utf-8")
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    "-I", str(header_dir), str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("Mini Racer production traffic and state host tests OK")
