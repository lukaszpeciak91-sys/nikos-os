// Deterministic ENT1/ENT2 migration and conservative fallback policy.
#include "entertainment_guard/guard.hpp"
#include <cassert>
using namespace nikos::entertainment_guard;
constexpr std::uint32_t minute = 60000U;
int main() {
    Guard fresh;
    fresh.restore({}, 1000, false, 0);
    assert(fresh.phase(1000) == Phase::Available);
    assert(!fresh.pending_clear());

    // ENT2 stores absolute UTC minute when the 20+60 cycle first started.
    constexpr std::uint32_t old_start = 100000U;
    const Snapshot old{true, true, old_start, false,
                       0, 0, false, false, true};
    Guard partial;
    partial.restore(old, 1000, true,
                    static_cast<std::uint64_t>(old_start + 10U) * 60U);
    assert(partial.phase(1000) == Phase::Window);
    assert(partial.used_ms() == 10U * minute);
    assert(partial.pending_write()); // Upgrade old record.
    Guard cooling;
    cooling.restore(old, 1000, true,
                    static_cast<std::uint64_t>(old_start + 30U) * 60U);
    assert(cooling.phase(1000) == Phase::Cooldown);
    assert(cooling.phase(1000 + 49U * minute) == Phase::Cooldown);
    assert(cooling.phase(1000 + 50U * minute) == Phase::Available);
    Guard expired;
    expired.restore(old, 1000, true,
                    static_cast<std::uint64_t>(old_start + 80U) * 60U);
    assert(expired.phase(1000) == Phase::Available);
    assert(expired.pending_clear());
    Guard backdated;
    backdated.restore(old, 1000, true,
                      static_cast<std::uint64_t>(old_start - 1U) * 60U);
    assert(backdated.phase(1000) == Phase::Cooldown);
    Guard untrusted;
    untrusted.restore(old, 1000, false, 0);
    assert(untrusted.phase(1000) == Phase::Cooldown);
    assert(untrusted.phase(1000 + Guard::kBreakMs - 1U) == Phase::Cooldown);
    assert(untrusted.phase(1000 + Guard::kBreakMs) == Phase::Available);
    return 0;
}
