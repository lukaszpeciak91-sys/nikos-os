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
#include <algorithm>
#include "mini_racer/traffic_rules.hpp"
using namespace nikos::mini_racer::rules;
int main() {
    static_assert(kEnemyCount == 3, "three cars");
    assert(speed_for_score(0) == 36U * 256);
    assert(speed_for_score(99) == 36U * 256);
    assert(speed_for_score(100) == 41U * 256);
    assert(speed_for_score(199) == 41U * 256);
    assert(speed_for_score(200) == 46U * 256);
    assert(speed_for_score(700) == 71U * 256);
    assert(speed_for_score(800) == 75U * 256);
    assert(speed_for_score(UINT32_MAX) == 75U * 256);

    Motion motion;
    std::uint64_t total = 0;
    std::uint32_t road = 0;
    Traffic traffic{};
    Random rng(42);
    initialize(traffic, 255, rng);
    for (int i = 0; i < 200; ++i) {
        const std::uint32_t dt = static_cast<std::uint32_t>((i % 4 + 1) * 7);
        const auto delta = motion.advance(36U * 256, dt);
        total += static_cast<std::uint64_t>(36U * 256) * dt;
        assert(!advance(traffic, delta, 2, rng));
        road += delta;
        assert(road == total / 1000U); // no fractional loss in either system
    }
    Motion tiny;
    std::uint32_t distance = 0;
    for (int i = 0; i < 1000; ++i) distance += tiny.advance(36U * 256, 1);
    assert(distance == 36U * 256);

    // Reproducible seed; multiple seeds give lane repeats and alternations.
    Random seed1(123), seed2(123);
    Traffic t1{}, t2{};
    initialize(t1, 255, seed1);
    initialize(t2, 255, seed2);
    for (std::size_t i=0;i<3;++i) {
        assert(t1[i].x_q8 == t2[i].x_q8);
        assert(t1[i].lane == t2[i].lane);
    }
    bool repeat=false, alternate=false, varied=false;
    for (std::uint32_t seed=1;seed<=100;++seed) {
        Random g(seed); Traffic t{}; initialize(t,255,g);
        for (int i=0;i<2;++i) {
            const auto gap=(t[i+1].x_q8-t[i].x_q8)/256;
            assert(gap>=kMinGapPx && gap<=kMaxGapPx);
            if (gap!=kMinGapPx) varied=true;
            if (t[i].lane==t[i+1].lane) repeat=true;
            else alternate=true;
        }
    }
    assert(repeat && alternate && varied);
    Random recycle(47); Traffic roadcars{}; initialize(roadcars,255,recycle);
    for (int step=0;step<25000;++step) {
        assert(!advance(roadcars,75U * 256 * 60 / 1000,2,recycle));
        for (std::size_t i=0;i<3;++i) for(std::size_t j=i+1;j<3;++j) {
            const auto difference=std::abs(roadcars[i].x_q8-roadcars[j].x_q8);
            // Once generated, cars have a safe longitudinal headway.
            assert(difference >= kMinGapPx*256);
        }
    }
    // Swept collision on a delayed frame must catch a car crossed over.
    Random sweep(123); initialize(roadcars,255,sweep);
    roadcars[0].x_q8=70*256;
    assert(advance(roadcars,75U*256*200/1000,roadcars[0].lane,sweep));
    // Pauses do not contribute elapsed time to the motion integrator.
    Motion resumed; assert(resumed.advance(75U*256,0)==0);
    assert(resumed.advance(75U*256,60)<=75U*256*60/1000+1);
    assert(pause(State::Playing)==State::Paused);
    assert(resume(State::Paused)==State::Playing);
    assert(hit(State::Paused)==State::Paused);
    return 0;
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
