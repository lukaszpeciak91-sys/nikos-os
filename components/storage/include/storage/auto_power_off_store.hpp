#pragma once

namespace nikos::storage {

class AutoPowerOffStore final {
public:
    bool load_enabled() const;
    bool save_enabled(bool enabled) const;
};

}  // namespace nikos::storage
