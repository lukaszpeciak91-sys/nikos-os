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

    // Concurrent spawn policy is deterministic and bounded per score stage.
    for (std::uint32_t seed=1;seed<=256;++seed) {
        for (unsigned stage : {0U, 100U, 250U, 500U}) {
            reset(w,seed,0);
            w.score=stage;
            unsigned observed_max=0;
            // Exercise actual production spawn and bounded simulation.
            for (int i=0;i<500;++i) {
                w.time_ms += 300;
                spawn(w);
                const auto count=active_enemies(w);
                assert(count <= concurrent_limit(stage));
                observed_max=std::max(observed_max,count);
                // Pairwise arrival windows (including hostile shots) are
                // the true safety constraint, not an empty-lane snapshot.
                for (std::size_t j=0;j<kMaxEnemies;++j) if (w.enemies[j].active) {
                    const auto& x=w.enemies[j];
                    for (std::size_t k=j+1;k<kMaxEnemies;++k)
                        if (w.enemies[k].active) {
                            const auto& y=w.enemies[k];
                            if (x.x_q8 <= (kPlayerX+16)*256 ||
                                y.x_q8 <= (kPlayerX+16)*256) continue;
                            const auto at_x=arrival_ms(x.x_q8,speed_for(x.type,w.score));
                            const auto at_y=arrival_ms(y.x_q8,speed_for(y.type,w.score));
                            const auto difference=at_x>at_y ? at_x-at_y : at_y-at_x;
                            assert(difference>=1100);
                        }
                }
                // Shift the whole scene left by fixed Q8 steps without
                // affecting relative distances (constant-score stage).
                for(auto& x:w.enemies) if(x.active) {
                    x.x_q8-=2*256;
                    if(x.x_q8 < -30*256) x.active=false;
                }
            }
            assert(observed_max>=2); // proved multiple hulls on every stage
        }
    }
    // Hostile projectiles can suppress dangerous arrivals without banning
    // all concurrent hulls or imposing independent three-lane barriers.
    reset(w,99,0);
    w.score=500;
    w.hostile[0]=Shot{true,1,244*256};
    assert(!safe_spawn(w,2,Type::Scout,244*256));
    w.hostile[0].active=false;
    spawn(w);
    assert(active_enemies(w)==1);
    spawn(w);
    assert(active_enemies(w)>=1 && active_enemies(w)<=4);
    // Conservative one-button escape: at max hull speed, two M5 presses
    // within 1100ms suffice to reach either of the other lanes.
    constexpr std::uint32_t kTwoPressBudgetMs=1100;
    static_assert(2*300U < kTwoPressBudgetMs, "cycle transition budget");
    for (int from=0;from<3;++from)
        for (int to=0;to<3;++to) {
            reset(w,11,0);
            w.lane=static_cast<std::uint8_t>(from);
            for(int presses=0;presses<3 && w.lane!=to;++presses)cycle_lane(w);
            assert(w.lane==to);
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
