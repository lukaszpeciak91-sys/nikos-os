#pragma once
#include "clock/warsaw_time_rules.hpp"

namespace nikos::clock {
// Each operation delegates to the existing Board/NVS action in firmware.
// An incomplete commit cannot authorize trusted UTC after a restart.
template <typename Operations>
bool commit_utc_checked(const warsaw::DateTime& utc, Operations& ops) {
    if (!warsaw::valid(utc) || !ops.available()) return false;
    if (!ops.save_pending()) return false;
    ops.mark_pending();
    if (!ops.invalidate_date_trust()) return false;
    if (!ops.write_date(utc)) return false;
    if (!ops.write_time(utc)) return false;
    warsaw::DateTime verified{};
    if (!ops.readback(verified)) return false;
    const auto elapsed = warsaw::seconds(verified) - warsaw::seconds(utc);
    if (elapsed < 0 || elapsed > 3) return false;
    if (!ops.save_utc_basis()) return false;
    if (!ops.restore_date_trust()) return false;
    ops.mark_trusted_utc();
    return true;
}
} // namespace nikos::clock
