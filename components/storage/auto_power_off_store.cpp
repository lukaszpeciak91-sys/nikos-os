#include "storage/auto_power_off_store.hpp"

#include <cstdint>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

namespace {

constexpr char kTag[] = "auto_off_store";
constexpr char kNamespace[] = "nikos";
constexpr char kEnabledKey[] = "auto_off";

}  // namespace

namespace nikos::storage {

bool AutoPowerOffStore::load_enabled() const
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(
        kNamespace,
        NVS_READONLY,
        &handle);
    if (result != ESP_OK) {
        ESP_LOGW(
            kTag,
            "Preference unavailable; using default ON: %s",
            esp_err_to_name(result));
        return true;
    }

    std::uint8_t stored = 1;
    result = nvs_get_u8(handle, kEnabledKey, &stored);
    nvs_close(handle);

    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return true;
    }
    if (result != ESP_OK || stored > 1U) {
        ESP_LOGW(kTag, "Invalid preference; using default ON");
        return true;
    }

    return stored == 1U;
}

bool AutoPowerOffStore::save_enabled(bool enabled) const
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(
        kNamespace,
        NVS_READWRITE,
        &handle);
    if (result == ESP_OK) {
        result = nvs_set_u8(
            handle,
            kEnabledKey,
            enabled ? 1U : 0U);
    }
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    if (handle != 0) {
        nvs_close(handle);
    }

    if (result != ESP_OK) {
        ESP_LOGE(
            kTag,
            "Failed to persist preference: %s",
            esp_err_to_name(result));
        return false;
    }

    return true;
}

}  // namespace nikos::storage
