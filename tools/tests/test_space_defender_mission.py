#!/usr/bin/env python3
"""Production Space Defender finite mission, deadline and seeded reachability."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
compiler=shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("C++17 compiler required")
source=r"""
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include "space_defender/rules.hpp"
using namespace nikos::space_defender::rules;
int main() {
    static_assert(kWaveMs==140000U && kInvasionMs==420000U);
    static_assert(kSessionMs==600000U);
    assert(wave_at(0)==1 && wave_at(139999)==1);
    assert(wave_at(140000)==2 && wave_at(279999)==2);
    assert(wave_at(280000)==3);
    for(unsigned seed=1;seed<=30;++seed) {
        World w{};reset(w,seed,0);w.mission_enabled=true;
        std::uint8_t highest_wave=1;
        std::uint32_t finish=0;
        // Legal input: only forwards M5 lane cycles with a 200ms reaction cadence.
        for(std::uint32_t t=0;t<590000 && w.phase==Phase::Playing;t+=100) {
            if(t%200U==0) {
                unsigned risk[3]={0,0,0};
                for(const auto& e:w.enemies) if(e.active) {
                    const auto x=e.x_q8/256;
                    if(x>=20 && x<=165) risk[e.lane]+=x<=100?5U:2U;
                }
                for(const auto& s:w.hostile) if(s.active) {
                    const auto x=s.x_q8/256;
                    if(x>=15 && x<=105) risk[s.lane]+=8U;
                }
                const auto next=static_cast<std::uint8_t>((w.lane+1U)%3U);
                if(risk[w.lane]>risk[next]) cycle_lane(w);
            }
            advance(w,100);
            highest_wave=std::max(highest_wave,w.wave);
            if(w.phase==Phase::Victory) finish=w.time_ms;
        }
        std::printf("seed=%u outcome=%u lives=%u wave=%u elapsed=%u score=%u\\n",
            seed,static_cast<unsigned>(w.phase),w.lives,highest_wave,finish,w.score);
        assert(highest_wave==3);
        assert(w.phase==Phase::Victory);
        assert(finish>=420000U && finish<=450000U);
        assert(threats_clear(w));
        assert(w.lives>0);
        assert(w.record>=w.score);
        const auto score=w.score,record=w.record,lives=w.lives;
        advance(w,500);
        assert(w.phase==Phase::Victory && w.score==score
            && w.record==record && w.lives==lives);
        const auto first_record=w.record;
        reset(w,seed,first_record);w.mission_enabled=true;
        assert(w.time_ms==0 && w.phase==Phase::Playing && w.wave==1);
        assert(w.record==first_record);
    }
    World loss{};reset(loss,42,0);loss.mission_enabled=true;
    loss.time_ms=kInvasionMs;loss.lives=1;damage(loss);
    assert(loss.phase==Phase::GameOver);
    substep(loss,20);
    assert(loss.phase==Phase::GameOver);
    World unresolved{};reset(unresolved,13,0);unresolved.mission_enabled=true;
    unresolved.time_ms=kInvasionMs-20;
    unresolved.hostile[0]=Shot{true,1,200*256};
    substep(unresolved,20);
    assert(unresolved.phase==Phase::Playing);
    // The owning app checks wall-clock deadline separately; sim-time does
    // not advance on pause, display-off or communicator preemption.
    World paused{};reset(paused,1,0);paused.mission_enabled=true;
    paused.phase=Phase::Paused;advance(paused,600000);
    assert(paused.time_ms==0);
}
"""
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp)/"mission.cpp"
    exe=Path(tmp)/"mission"
    p.write_text(source,encoding="utf-8")
    subprocess.run([compiler,"-std=c++17","-O2","-Wall","-Wextra","-Werror",
                    "-I",str(root/"apps/space_defender/include"),
                    str(p),"-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
print("Finite Space Defender mission tests PASS")
