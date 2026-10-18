#include "mini_racer/race_rules.hpp"
#include "mini_racer/traffic_rules.hpp"
#include <cassert>
using namespace nikos::mini_racer;
int main() {
    race::Round r{};
    r.begin(100U);
    assert(r.stage()==1 && r.progress_percent()==0);
    const auto advance=[&](std::uint32_t pixels,std::uint32_t at) {
        const auto before=r.progress_percent();
        r.advance(pixels*256U,false,at);
        assert(r.progress_percent()>=before && r.progress_percent()<=100);
    };
    advance(6999,1000);assert(r.stage()==1);
    advance(1,1001);assert(r.stage()==2);
    advance(7000,1002);assert(r.stage()==3);
    advance(6999,1003);assert(r.outcome==race::Outcome::Racing);
    advance(1,1004);assert(r.outcome==race::Outcome::Victory);
    r.advance(100U,true,600100U);assert(r.outcome==race::Outcome::Victory);
    race::Round collision{};collision.begin(10);
    collision.advance(race::kFinishPx*256U,true,11);
    assert(collision.outcome==race::Outcome::Collision);
    collision.advance(race::kFinishPx*256U,false,12);
    assert(collision.outcome==race::Outcome::Collision);
    race::Round timeout{};timeout.begin(100);
    timeout.advance(race::kFinishPx*256U,false,600100);
    assert(timeout.outcome==race::Outcome::Timeout);
    race::Round pause{};pause.begin(400);
    // Pausing and communicators do not change the monotonic round start.
    pause.advance(0,false,600400);
    assert(pause.outcome==race::Outcome::Timeout);
    timeout.begin(600500);
    assert(timeout.outcome==race::Outcome::Racing);
    assert(!timeout.deadline(600501) && timeout.deadline(1200500));
    timeout.begin(0); assert(timeout.progress_percent()==0);
    // Real production traffic speed + motion integrator: no artificial timer.
    race::Round clean{};clean.begin(0);
    rules::Motion motion{};
    std::uint32_t simulated_ms=0;
    for(;simulated_ms<600000 && clean.outcome==race::Outcome::Racing;
        simulated_ms+=50U) {
        const auto score=simulated_ms/100U;
        const auto displacement=motion.advance(rules::speed_for_score(score),50U);
        clean.advance(displacement,false,simulated_ms);
    }
    assert(clean.outcome==race::Outcome::Victory);
    assert(simulated_ms>=4U*60000U && simulated_ms<=7U*60000U);
    // Traffic remains independently collision tested by test_mini_racer.py.
    static_assert(race::kFinishPx*256U<UINT32_MAX);
    static_assert(race::kDeadlineMs==10U*60000U);
}
