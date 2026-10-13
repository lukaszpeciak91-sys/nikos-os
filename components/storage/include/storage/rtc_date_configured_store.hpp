#pragma once

namespace nikos::storage {

class RtcDateConfiguredStore final {
public:
    bool load_configured(bool& configured) const;
    bool save_configured(bool configured) const;
};

}  // namespace nikos::storage
