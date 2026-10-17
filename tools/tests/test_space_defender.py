#!/usr/bin/env python3
"""Compile and run deterministic production Space Defender rules as C++17."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("SKIP: C++17 host compiler unavailable")
source = r"""
#include <cassert>
#include <cstdint>
#include <algorithm>
#include "space_defender/rules.hpp"
using namespace nikos::space_defender::rules;
int main() {
    static_assert(kMaxEnemies==4 && kMaxShots==8 && kMaxHostileShots==4);
    static_assert(kFireIntervalMs==350 && kInvulnerableMs==1500);
    World w{};
    reset(w,3,90);
    assert(w.lives==3 && w.lane==1 && w.record==90 && w.score==0);
    cycle_lane(w); assert(w.lane==2);
    cycle_lane(w); assert(w.lane==0);
    cycle_lane(w); assert(w.lane==1);
    advance(w,120);
    assert(std::none_of(w.shots.begin(),w.shots.end(),[](Shot s){return s.active;}));
    advance(w,120);advance(w,110);
    assert(std::count_if(w.shots.begin(),w.shots.end(),[](Shot s){return s.active;})==1);
    const auto first_lane=w.shots[0].lane;
    cycle_lane(w);
    assert(w.shots[0].lane==first_lane);
    const auto pos=w.shots[0].x_q8;
    advance(w,120);
    assert(w.shots[0].x_q8>pos);
    // Fractional Q8 motion conserves displacement over uneven increments.
    std::uint32_t rem=0;std::uint64_t total=0;std::uint32_t dx=0;
    for (int i=0;i<1000;++i) {
        std::uint32_t dt=(i%4)+1;
        dx+=move_q8(27,dt,rem);
        total+=static_cast<std::uint64_t>(27)*256*dt;
    }
    assert(dx==total/1000 && rem==total%1000);
    assert(choose_type(w.rng,99)==Type::Scout);
    for (unsigned score: {100U,249U,250U,499U,500U})
        for (int i=0;i<100;++i) {
            auto t=choose_type(w.rng,score);
            if(score<250)assert(t!=Type::Shooter);
        }
    assert(hp_for(Type::Scout)==1 && hp_for(Type::Armored)==3 && hp_for(Type::Shooter)==2);
    assert(points_for(Type::Scout)==10 && points_for(Type::Armored)==30
        && points_for(Type::Shooter)==25);
    assert(speed_for(Type::Scout,500)>speed_for(Type::Scout,499));

    // Three distinct valid bullet hits remove armor and award exactly once.
    reset(w,8,0);
    w.next_spawn_ms=UINT32_MAX;
    w.enemies[0]=Enemy{};
    auto& e=w.enemies[0];
    e.active=true;e.type=Type::Armored;e.hp=3;e.lane=1;e.x_q8=88*256;
    for(int i=0;i<3;++i) {
        Shot& shot=w.shots[i];
        shot=Shot{true,1,76*256};
        advance(w,100);
        assert(e.active==(i<2));
        assert(e.hp==2-i);
    }
    assert(w.score==30 && w.record==30);
    advance(w,100);assert(w.score==30);

    // Enemy shots retain their lane, do not home, and move left.
    reset(w,10,0);
    w.next_spawn_ms=UINT32_MAX;
    w.hostile[0]=Shot{true,0,130*256};
    auto before=w.hostile[0].x_q8;
    cycle_lane(w);
    advance(w,100);
    assert(w.hostile[0].lane==0 && w.hostile[0].x_q8<before);

    // One hit, immunity for 1500ms, and exactly-once game-over.
    reset(w,10,0);w.next_spawn_ms=UINT32_MAX;
    damage(w);assert(w.lives==2);
    damage(w);assert(w.lives==2);
    advance(w,120);
    damage(w);assert(w.lives==2);
    // advance uses a deliberate scheduling clamp; simulate normal intervals.
    for(int i=0;i<12;++i)advance(w,120);
    assert(w.time_ms>=1500);
    damage(w);assert(w.lives==1);
    for(int i=0;i<13;++i)advance(w,120);
    damage(w);assert(w.lives==0 && w.phase==Phase::GameOver);
    damage(w);assert(w.lives==0);
    reset(w,12,w.record);
    assert(w.lives==3 && w.phase==Phase::Playing && w.score==0);
    assert(std::none_of(w.enemies.begin(),w.enemies.end(),[](Enemy x){return x.active;}));
    assert(std::none_of(w.hostile.begin(),w.hostile.end(),[](Shot x){return x.active;}));
    assert(w.invulnerable_until_ms==0);

    // Seeded bounded spawning, no three-lane barrier, fixed pools.
    for(std::uint32_t seed=1;seed<=256;++seed) {
        reset(w,seed,0);
        for(int i=0;i<5000;++i) {
            w.lane=2; // exercise traffic without relying on random controller.
            if(w.phase!=Phase::Playing)break;
            advance(w,120);
            unsigned alive=0,occupied=0;
            for(auto x:w.enemies)if(x.active){++alive;occupied|=1U<<x.lane;}
            for(auto x:w.hostile)if(x.active)occupied|=1U<<x.lane;
            assert(alive<=1 && occupied!=7U);
            assert(std::count_if(w.shots.begin(),w.shots.end(),
                [](Shot x){return x.active;})<=static_cast<int>(kMaxShots));
        }
    }
    // Sweep: a delayed frame crossing player's collision box is damage.
    reset(w,13,0);
    w.next_spawn_ms=UINT32_MAX;
    w.hostile[0]=Shot{true,1,49*256};
    advance(w,120); // 3.24px left, still outside
    assert(w.lives==3);
    for(int i=0;i<10;++i)advance(w,120);
    assert(w.lives==2);
    // Paused updates cannot advance timers or cause a movement spike.
    reset(w,14,4);
    auto snapshot=w.time_ms;
    w.phase=Phase::Paused;
    advance(w,UINT32_MAX);
    assert(w.time_ms==snapshot);
    w.phase=Phase::Playing;
    advance(w,0);assert(w.time_ms==snapshot);
    advance(w,UINT32_MAX);assert(w.time_ms==snapshot+kMaxElapsedMs);
    // The wall clock session rule remains external: it includes pauses.
    assert(kSessionMs==600000U);
    return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    target = Path(tmp)/"space_defender_host.cpp"
    binary = Path(tmp)/"space_defender_host"
    target.write_text(source, encoding="utf-8")
    subprocess.run([compiler, "-std=c++17", "-O2", "-Wall", "-Wextra",
                    "-Werror", "-I", str(root/"apps/space_defender/include"),
                    str(target), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("Space Defender production rules host tests PASS")
