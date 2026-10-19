#pragma once
#include <cstdint>

namespace nikos::storage {
struct EntertainmentCycle {
    bool active = false;
    bool civil_valid = false;
    std::uint32_t civil_start = 0; // ENT1/ENT2 minutes, migration only.
    bool warning_shown = false;
    bool legacy_local = false;
    std::uint32_t used_ms = 0;
    std::uint64_t anchor_utc_s = 0;
    bool paused = false;
    bool cooldown = false;
    bool legacy = false;
};
class EntertainmentCycleStore {
public:
    bool load(EntertainmentCycle& cycle) const;
    bool save(const EntertainmentCycle& cycle) const;
    bool clear() const;
};
}
