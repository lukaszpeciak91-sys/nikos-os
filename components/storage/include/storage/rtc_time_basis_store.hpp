#pragma once
#include <cstdint>
namespace nikos::storage {
enum class RtcTimeBasis : std::uint8_t { LegacyLocal, Pending, Utc, Uncertain };
class RtcTimeBasisStore final {
public:
    RtcTimeBasis load() const;
    bool save(RtcTimeBasis basis) const;
};
} // namespace nikos::storage
