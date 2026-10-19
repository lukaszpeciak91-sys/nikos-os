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
    cycle_lane(w); assert(w.lane==1);
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

    // At 499, a safe check must reserve the imminent +6px/s boost.
    reset(w,41,0);
    w.score=499;
    w.enemies[0]=Enemy{};
    w.enemies[0].active=true;
    w.enemies[0].type=Type::Armored;
    w.enemies[0].lane=0;
    w.enemies[0].x_q8=235*256;
    const auto armor=hull_window(w,Type::Armored,w.enemies[0].x_q8);
    assert(armor.first < armor.last);
    const auto fast=hull_window(w,Type::Scout,380*256);
    assert(fast.first<fast.last);
    // An apparently safe point prediction can be unsafe with boost timing.
    bool found_unsafe_point=false;
    for(int x=270;x<=560;++x) {
        const auto predicted=arrival_ms(x*256,speed_for(Type::Scout,w.score));
        const auto other=arrival_ms(w.enemies[0].x_q8,speed_for(Type::Armored,w.score));
        const auto diff=predicted>other?predicted-other:other-predicted;
        if(diff>=1100 && !safe_spawn(w,2,Type::Scout,x*256)) {
            found_unsafe_point=true;break;
        }
    }
    assert(found_unsafe_point);

    // Future shooter rounds must not overlap an active hull: a warning
    // does not model a fired hostile projectile until later.
    reset(w,17,0);w.score=500;
    w.enemies[0].active=true;
    w.enemies[0].type=Type::Shooter;
    w.enemies[0].lane=1;
    w.enemies[0].x_q8=220*256;
    assert(!safe_spawn(w,0,Type::Scout,540*256));
    w.enemies[0].active=false;
    w.hostile[0]=Shot{true,1,210*256};
    assert(!safe_spawn(w,0,Type::Scout,290*256));
    w.hostile[0].active=false;
    assert(safe_spawn(w,0,Type::Scout,540*256));

    // Multiple real moving enemies, with faster scout chasing a slower
    // armored hull. Speed changes exactly when score crosses 500.
    reset(w,77,0);
    w.score=499;w.next_spawn_ms=UINT32_MAX;
    w.enemies[0]=Enemy{};
    w.enemies[0].active=true;w.enemies[0].type=Type::Armored;
    w.enemies[0].hp=3;w.enemies[0].lane=0;w.enemies[0].x_q8=180*256;
    w.enemies[1]=Enemy{};
    w.enemies[1].active=true;w.enemies[1].type=Type::Scout;
    w.enemies[1].hp=1;w.enemies[1].lane=1;w.enemies[1].x_q8=280*256;
    w.lane=2; // legal transition from middle, never an impossible lane.
    const auto a0=w.enemies[0].x_q8,b0=w.enemies[1].x_q8;
    for(int i=0;i<10;++i)advance(w,120);
    assert(w.enemies[0].x_q8<a0 && w.enemies[1].x_q8<b0);
    const auto gap0=b0-a0;
    const auto gap1=w.enemies[1].x_q8-w.enemies[0].x_q8;
    assert(gap1<gap0);
    w.score=500;
    const auto beforeA=w.enemies[0].x_q8;
    const auto beforeB=w.enemies[1].x_q8;
    advance(w,100);
    assert(w.enemies[0].x_q8<beforeA-26*256*100/1000);
    assert(w.enemies[1].x_q8<beforeB-38*256*100/1000);

    // Shooter warning, rate-limited firing, then a left-moving projectile.
    reset(w,11,0);w.score=250;w.next_spawn_ms=UINT32_MAX;
    w.lane=0;
    w.enemies[0]=Enemy{};
    w.enemies[0].active=true;w.enemies[0].type=Type::Shooter;
    w.enemies[0].hp=2;w.enemies[0].lane=2;
    w.enemies[0].x_q8=210*256;
    w.enemies[0].next_shot_ms=40;
    substep(w,20);substep(w,20);
    assert(w.enemies[0].warning_ms>w.time_ms);
    assert(std::none_of(w.hostile.begin(),w.hostile.end(),[](Shot s){return s.active;}));
    for(int i=0;i<16;++i)substep(w,20);
    assert(w.enemies[0].warning_ms==0);
    const auto hostileLane=w.hostile[0].lane;
    assert(w.hostile[0].active && hostileLane==2);
    const auto hostileX=w.hostile[0].x_q8;
    advance(w,100);
    assert(w.hostile[0].x_q8<hostileX && w.hostile[0].lane==hostileLane);
    w.enemies[0].active=false;
    advance(w,80);
    assert(w.hostile[0].active);

    // Concurrent hulls and hostile bullet never exceed pools.
    for(std::uint32_t seed=1;seed<=64;++seed) {
        reset(w,seed,0);
        w.score=500;
        w.lane=1;
        bool multi=false;
        for(int i=0;i<2000;++i) {
            if(w.phase!=Phase::Playing)break;
            // Legal lane cycling to dodge near-term hazards, no lane=3.
            if(i%12==0)cycle_lane(w);
            advance(w,120);
            const auto count=active_enemies(w);
            assert(count<=concurrent_limit(w.score) && count<=kMaxEnemies);
            if(count>=2)multi=true;
            assert(std::count_if(w.shots.begin(),w.shots.end(),
                [](Shot x){return x.active;})<=static_cast<int>(kMaxShots));
            assert(std::count_if(w.hostile.begin(),w.hostile.end(),
                [](Shot x){return x.active;})<=static_cast<int>(kMaxHostileShots));
        }
        // Multi-spawn capability is tested explicitly below without
        // requiring a surviving autopilot through every random seed.
        (void)multi;
    }
    // Bounded spawn can accept concurrent hulls from predictable seeds,
    // without custom placement of a second enemy or out-of-range lanes.
    bool observed_multi=false;
    for(std::uint32_t seed=1;seed<=512;++seed) {
        reset(w,seed,0);
        w.score=250;
        spawn(w);
        for(int i=0;i<25;++i) {
            substep(w,20);
            spawn(w);
            if(active_enemies(w)>=2) {observed_multi=true;break;}
        }
        if(observed_multi)break;
    }
    assert(observed_multi);

    // Cycling both intermediate lanes: immediate TOP->BOTTOM requires
    // exactly two M5 short events. The time for those presses is assumed,
    // not guaranteed by firmware or hardware.
    reset(w,5,0);w.lane=0;
    cycle_lane(w);assert(w.lane==1);
    cycle_lane(w);assert(w.lane==2);
    cycle_lane(w);assert(w.lane==0);
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
