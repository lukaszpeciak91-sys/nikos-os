// Existing ENT1 behavior contract. PR A intentionally does not migrate NVS.
#include "entertainment_guard/guard.hpp"
#include <cassert>
using namespace nikos::entertainment_guard;
int main() {
    Guard fresh;
    fresh.restore({},1000,false,0);
    assert(fresh.phase(1000)==Phase::Available); // No saved cycle, no cooldown.
    assert(!fresh.pending_clear() && !fresh.pending_write());
    Guard invalid;
    invalid.restore({true,false,0,false},1000,false,0);
    assert(invalid.phase(1000)==Phase::Cooldown);
    assert(invalid.phase(1000+Guard::kBreakMs-1)==Phase::Cooldown);
    assert(invalid.phase(1000+Guard::kBreakMs)==Phase::Available);
    assert(invalid.pending_clear());
    Guard saved;
    saved.restore({true,true,100000,false},1000,true,100010);
    assert(saved.phase(1000)==Phase::Window);
    assert(saved.phase(1000+10*60*1000)==Phase::Cooldown);
    assert(saved.phase(1000+70*60*1000)==Phase::Available);
    Guard backwards;
    backwards.restore({true,true,100000,false},1000,true,99900);
    assert(backwards.phase(1000)==Phase::Cooldown);
    Guard future;
    future.restore({true,true,100000,false},1000,true,100080);
    assert(future.phase(1000)==Phase::Available && future.pending_clear());
    // These tests document the present risks; migration must distinguish
    // active legacy ENT1 local civil minutes from future UTC-based ENT2.
}
