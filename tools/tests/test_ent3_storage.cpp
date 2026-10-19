// Host NVS fault injection against the real ENT3 storage implementation.
#include <cassert>
#include <cstdint>
#include <cstring>
#include "nvs.h"
#include "storage/entertainment_cycle_store.hpp"
#include "../../components/storage/entertainment_cycle_store.cpp"

using nikos::storage::EntertainmentCycle;
using nikos::storage::EntertainmentCycleStore;

int main() {
    fake_nvs::reset();
    EntertainmentCycleStore store;
    EntertainmentCycle read{};
    assert(store.load(read) && !read.active);

    EntertainmentCycle partial{};
    partial.active = true;
    partial.civil_valid = true;
    partial.anchor_utc_s = 100000U;
    partial.used_ms = 8U * 60000U;
    partial.paused = true;
    partial.warning_shown = true;
    assert(store.save(partial));
    assert(fake_nvs::exists && fake_nvs::durable.size() == 24U);
    assert(store.load(read));
    assert(read.active && !read.legacy && read.civil_valid);
    assert(read.used_ms == partial.used_ms && read.paused);
    assert(read.warning_shown && read.anchor_utc_s == 100000U);

    // NVS error paths must be reported, not silently treated as commits.
    const auto known_good = fake_nvs::durable;
    fake_nvs::fail_set = true;
    assert(!store.save(partial));
    fake_nvs::fail_set = false;
    assert(fake_nvs::durable == known_good);
    fake_nvs::fail_commit = true;
    assert(!store.save(partial));
    fake_nvs::fail_commit = false;
    assert(fake_nvs::durable == known_good);
    fake_nvs::fail_open = true;
    assert(!store.load(read) && read.active);
    fake_nvs::fail_open = false;
    fake_nvs::fail_get = true;
    assert(!store.load(read) && read.active);
    fake_nvs::fail_get = false;

    fake_nvs::durable[0] = 0; // Unknown marker must fail closed.
    assert(!store.load(read) && read.active);
    fake_nvs::durable = known_good;
    assert(store.load(read));
    fake_nvs::fail_erase = true;
    assert(!store.clear());
    fake_nvs::fail_erase = false;
    assert(fake_nvs::durable == known_good);
    fake_nvs::fail_commit = true;
    assert(!store.clear());
    fake_nvs::fail_commit = false;
    assert(fake_nvs::durable == known_good);
    assert(store.clear());
    assert(store.load(read) && !read.active);

    // Both 12-byte legacy formats are readable without treating them as new.
    for (std::uint32_t marker : {0x454E5431U, 0x454E5432U}) {
        fake_nvs::durable.assign(12U, 0U);
        fake_nvs::exists = true;
        const std::uint32_t old_minutes = 12345U;
        std::memcpy(fake_nvs::durable.data(), &marker, sizeof(marker));
        std::memcpy(fake_nvs::durable.data() + 4U,
                    &old_minutes, sizeof(old_minutes));
        fake_nvs::durable[8] = 1U;
        fake_nvs::durable[9] = 1U;
        assert(store.load(read));
        assert(read.legacy && read.active && read.civil_valid);
        assert(read.warning_shown && read.civil_start == old_minutes);
        assert(read.legacy_local == (marker == 0x454E5431U));
    }
    fake_nvs::durable.resize(11U);
    assert(!store.load(read) && read.active);
    return 0;
}
