#include "storage/entertainment_cycle_store.hpp"
#include <cstddef>
#include "nvs.h"
#include "esp_log.h"

namespace nikos::storage {
namespace {
constexpr char kNamespace[] = "nikos";
constexpr char kKey[] = "ent_cycle";
constexpr std::uint32_t kLegacyMarker = 0x454E5431U; // ENT1, local minutes
constexpr std::uint32_t kUtcMarker = 0x454E5432U; // ENT2, UTC minutes
constexpr std::uint32_t kSessionMarker = 0x454E5433U; // ENT3, UTC seconds
constexpr std::uint32_t kAllowanceMs = 20U * 60U * 1000U;
constexpr std::uint8_t kValid = 1U, kWarned = 2U, kPaused = 4U, kCooldown = 8U;
struct LegacyRecord {
    std::uint32_t marker;
    std::uint32_t civil_start;
    std::uint8_t civil_valid;
    std::uint8_t warning_shown;
    std::uint8_t reserved[2];
};
struct Record {
    std::uint32_t marker;
    std::uint32_t used_ms;
    std::uint64_t anchor_utc_s;
    std::uint8_t flags;
    std::uint8_t reserved[7];
};
static_assert(sizeof(LegacyRecord) == 12U, "ENT1/2 ABI");
static_assert(sizeof(Record) == 24U, "ENT3 ABI");
bool write(const Record* record) {
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW("ent_cycle", "NVS open failed: %d", err);
        return false;
    }
    if (record) err = nvs_set_blob(handle, kKey, record, sizeof(*record));
    else err = nvs_erase_key(handle, kKey);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    if (err != ESP_OK) ESP_LOGW("ent_cycle", "NVS commit failed: %d", err);
    return err == ESP_OK;
}
} // namespace

bool EntertainmentCycleStore::load(EntertainmentCycle& cycle) const {
    cycle = {};
    nvs_handle_t handle = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return true;
    if (err != ESP_OK) { cycle.active = true; return false; }

    std::size_t size = 0;
    err = nvs_get_blob(handle, kKey, nullptr, &size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return true;
    }
    if (err != ESP_OK || (size != sizeof(LegacyRecord) && size != sizeof(Record))) {
        nvs_close(handle);
        cycle.active = true;
        return false;
    }

    if (size == sizeof(LegacyRecord)) {
        LegacyRecord old{};
        err = nvs_get_blob(handle, kKey, &old, &size);
        nvs_close(handle);
        if (err != ESP_OK || size != sizeof(old)
            || (old.marker != kLegacyMarker && old.marker != kUtcMarker)
            || old.civil_valid > 1U || old.warning_shown > 1U) {
            cycle.active = true;
            return false;
        }
        cycle.active = true;
        cycle.legacy = true;
        cycle.legacy_local = old.marker == kLegacyMarker;
        cycle.civil_start = old.civil_start;
        cycle.civil_valid = old.civil_valid != 0U;
        cycle.warning_shown = old.warning_shown != 0U;
        return true;
    }
    Record saved{};
    err = nvs_get_blob(handle, kKey, &saved, &size);
    nvs_close(handle);
    if (err != ESP_OK || size != sizeof(saved)
        || saved.marker != kSessionMarker
        || (saved.flags & ~0x0FU) != 0U
        || saved.used_ms > kAllowanceMs
        || ((saved.flags & kValid) == 0U && saved.anchor_utc_s != 0U)
        || ((saved.flags & kValid) != 0U && saved.anchor_utc_s == 0U)) {
        cycle.active = true;
        return false;
    }
    cycle.active = true;
    cycle.civil_valid = (saved.flags & kValid) != 0U;
    cycle.warning_shown = (saved.flags & kWarned) != 0U;
    cycle.paused = (saved.flags & kPaused) != 0U;
    cycle.cooldown = (saved.flags & kCooldown) != 0U;
    cycle.used_ms = saved.used_ms;
    cycle.anchor_utc_s = saved.anchor_utc_s;
    return true;
}

bool EntertainmentCycleStore::save(const EntertainmentCycle& cycle) const {
    if (!cycle.active) return clear();
    const Record saved{
        kSessionMarker,
        cycle.used_ms,
        cycle.civil_valid ? cycle.anchor_utc_s : 0U,
        static_cast<std::uint8_t>(
            (cycle.civil_valid ? kValid : 0U)
            | (cycle.warning_shown ? kWarned : 0U)
            | (cycle.paused ? kPaused : 0U)
            | (cycle.cooldown ? kCooldown : 0U)),
        {}};
    return write(&saved);
}
bool EntertainmentCycleStore::clear() const { return write(nullptr); }
}
