// g++ -std=c++17 -Wall -Wextra -Werror -Icomponents/clock/include
//     -Icomponents/entertainment_guard/include tools/tests/test_clock_utc_migration.cpp -o /tmp/utc_migration
// /tmp/utc_migration
#include "clock/legacy_civil_minutes.hpp"
#include "entertainment_guard/guard.hpp"
#include <cassert>
using namespace nikos::clock;
using namespace nikos::clock::warsaw;
using namespace nikos::entertainment_guard;
constexpr std::uint32_t local_minutes(DateTime time) {
    return static_cast<std::uint32_t>(seconds(time)/60);
}
int main() {
    std::uint32_t utc_start=0;
    // Legacy winter cycle translates one hour back; summer two hours.
    assert(legacy_local_minutes_to_utc(
        local_minutes({2026,1,15,12,0,0}),utc_start));
    assert(utc_start==local_minutes({2026,1,15,11,0,0}));
    assert(legacy_local_minutes_to_utc(
        local_minutes({2026,7,15,12,0,0}),utc_start));
    assert(utc_start==local_minutes({2026,7,15,10,0,0}));
    assert(!legacy_local_minutes_to_utc(
        local_minutes({2026,10,25,2,30,0}),utc_start));
    assert(!legacy_local_minutes_to_utc(
        local_minutes({2026,3,29,2,30,0}),utc_start));
    assert(!legacy_local_minutes_to_utc(0,utc_start));

    // Existing behavior remains monotonic once boot recovery finishes.
    Guard none;
    none.restore({},1000,false,0);
    assert(none.phase(1000)==Phase::Available);
    Guard active;
    const std::uint32_t start=local_minutes({2026,7,15,10,0,0});
    active.restore({true,true,start,false},1000,true,start+30);
    assert(active.phase(1000)==Phase::Cooldown);
    assert(active.phase(1000+49*60000)==Phase::Cooldown);
    assert(active.phase(1000+50*60000)==Phase::Available);
    Guard expired;
    expired.restore({true,true,start,false},1000,true,start+80);
    assert(expired.phase(1000)==Phase::Available);
    assert(expired.pending_clear());
    Guard uncertain;
    uncertain.restore({true,false,0,false},1000,false,0);
    assert(uncertain.phase(1000)==Phase::Cooldown);
    assert(uncertain.phase(1000+Guard::kBreakMs)==Phase::Available);
    // Intentional accepted limitation: if restarted before fallback expiry,
    // an untrusted active persisted cycle can start another 60m fallback.
}
