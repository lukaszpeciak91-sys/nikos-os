#include "space_defender/rules.hpp"
#include <cassert>
#include <algorithm>
#include <cstdint>
using namespace nikos::space_defender::rules;
int main() {
    static_assert(kInvasionMs==420000 && kSessionMs==600000);
    assert(wave_at(0)==1 && wave_at(139999)==1);
    assert(wave_at(140000)==2 && wave_at(279999)==2);
    assert(wave_at(280000)==3 && wave_at(420000)==3);
    // One-button movement must bounce: top -> mid -> bottom -> mid -> top.
    World lanes{};
    reset(lanes,12,0);
    lanes.lane = 0;
    for (const auto expected : {1U, 2U, 1U, 0U, 1U, 2U}) {
        cycle_lane(lanes);
        assert(lanes.lane == expected);
    }
    lanes.phase = Phase::Paused;
    cycle_lane(lanes);
    assert(lanes.lane == 2U);
    reset(lanes,12,0);
    assert(lanes.lane == 1U);
    cycle_lane(lanes);
    assert(lanes.lane == 2U);
    World w{};reset(w,12,0);w.mission_enabled=true;
    assert(invasion_active(w));
    w.time_ms=kInvasionMs;
    assert(!invasion_active(w));
    spawn(w);assert(active_enemies(w)==0);
    assert(threats_clear(w));
    w.time_ms=kInvasionMs-20;
    w.next_spawn_ms=kInvasionMs+999;
    substep(w,20);
    assert(w.phase==Phase::Victory);
    substep(w,20);assert(w.phase==Phase::Victory);
    reset(w,2,10);w.mission_enabled=true;
    w.time_ms=kInvasionMs-20;
    w.hostile[0]=Shot{true,1,100*256};
    w.next_spawn_ms=UINT32_MAX;
    substep(w,20);
    assert(w.phase==Phase::Playing && !threats_clear(w));
    for(int i=0;i<3000 && w.phase==Phase::Playing;++i)advance(w,120);
    assert(w.phase==Phase::Victory && threats_clear(w));
    reset(w,2,10);w.mission_enabled=true;
    w.time_ms=kInvasionMs-20;
    w.lives=1;
    w.hostile[0]=Shot{true,1,49*256};
    w.next_spawn_ms=UINT32_MAX;
    for(int i=0;i<60 && w.phase==Phase::Playing;++i)advance(w,120);
    assert(w.phase==Phase::GameOver);
    // A legitimate finite mission uses the unmodified combat engine.
    for(std::uint32_t seed=1;seed<=30;++seed) {
        reset(w,seed,0);w.mission_enabled=true;
        std::uint32_t elapsed=0;
        std::uint32_t next_wave2=0,next_wave3=0;
        while(w.phase==Phase::Playing && elapsed<kSessionMs) {
            // Cautious human-like policy: at most one lane press per 240 ms.
            // Prefer a safe lane on upcoming enemy/hazard approach.
            if(elapsed%240U==0U) {
                const std::uint8_t target=static_cast<std::uint8_t>((w.lane+1U)%3U);
                bool danger=false;
                for(const auto& e:w.enemies)
                    if(e.active && e.lane==w.lane && e.x_q8<105*256
                        && e.x_q8>20*256) danger=true;
                for(const auto& h:w.hostile)
                    if(h.active && h.lane==w.lane && h.x_q8<120*256
                        && h.x_q8>20*256) danger=true;
                if(danger) cycle_lane(w);
                (void)target;
            }
            advance(w,120);
            elapsed+=120;
            if(w.wave==2 && !next_wave2)next_wave2=elapsed;
            if(w.wave==3 && !next_wave3)next_wave3=elapsed;
        }
        assert(next_wave2>=kWaveMs && next_wave2<=kWaveMs+120);
        assert(next_wave3>=2*kWaveMs && next_wave3<=2*kWaveMs+120);
        assert(w.phase==Phase::Victory);
        assert(elapsed>=420000 && elapsed<=450000);
        assert(w.lives>0 && w.record>=w.score);
        assert(threats_clear(w));
    }
}
