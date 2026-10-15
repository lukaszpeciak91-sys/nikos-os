#pragma once
#include <cstdint>
namespace nikos::storage {
class SignalVolumeStore final {
public:
    std::uint8_t load_level() const;
    bool save_level(std::uint8_t level) const;
};
}
