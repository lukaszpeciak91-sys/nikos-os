#include "storage/rtc_date_configured_store.hpp"

#include <cstdint>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

namespace {

constexpr char kTag[] = "rtc_date_store";
constexpr char kNamespace[] = "nikos";
constexpr char kConfiguredKey[] = "rtc_date_cfg";

}  // namespace

namespace nikos::storage {

bool RtcDateConfiguredStore::load_configured(bool& configured) const
{
    configured = false;

    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(
        kNamespace,
        NVS_READONLY,
        &handle);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return true;
    }
    if (result != ESP_OK) {
        ESP_LOGW(
            kTag,
            "Date trust marker unavailable; using default false: %s",
            esp_err_to_name(result));
        return false;
    }

    std::uint8_t stored = 0;
    result = nvs_get_u8(handle, kConfiguredKey, &stored);
    nvs_close(handle);

    if (result == ESP_ERR_NVS_NOT_FOUND) {
        return true;
    }
    if (result != ESP_OK || stored > 1U) {
        ESP_LOGW(kTag, "Invalid date trust marker; using false");
        return false;
    }

    configured = stored == 1U;
    return true;
}

bool RtcDateConfiguredStore::save_configured(bool configured) const
{
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(
        kNamespace,
        NVS_READWRITE,
        &handle);
    if (result == ESP_OK) {
        result = nvs_set_u8(
            handle,
            kConfiguredKey,
            configured ? 1U : 0U);
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
            "Failed to persist date trust marker: %s",
            esp_err_to_name(result));
        return false;
    }

    return true;
}

}  // namespace nikos::storage
