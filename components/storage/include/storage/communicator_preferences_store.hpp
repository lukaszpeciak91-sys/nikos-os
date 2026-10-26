#pragma once

#include <cstdint>

namespace nikos::storage {

// Per-device NVS preferences, independent of the Communicator enabled session.
class CommunicatorPreferencesStore final {
public:
    std::uint8_t load_view() const;       // 0=LISTA, 1=POJEDYNCZO, 2=TYLKO SYGNAL
    std::uint8_t load_radio_mode() const; // 0=STANDARD, 1=LR
    bool save_view(std::uint8_t view) const;
    bool save_radio_mode(std::uint8_t mode) const;
};

}  // namespace nikos::storage
