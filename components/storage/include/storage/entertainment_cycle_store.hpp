#pragma once
#include <cstdint>
namespace nikos::storage {
struct EntertainmentCycle {
    bool active=false;
    bool civil_valid=false;
    std::uint32_t civil_start=0;
};
class EntertainmentCycleStore {
public:
    bool load(EntertainmentCycle& cycle) const;
    bool save(const EntertainmentCycle& cycle) const;
    bool clear() const;
};
}
