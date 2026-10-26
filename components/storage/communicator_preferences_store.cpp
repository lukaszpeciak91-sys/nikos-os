#include "storage/communicator_preferences_store.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

namespace {
constexpr char kTag[] = "comm_prefs";
constexpr char kNamespace[] = "nikos";
constexpr char kViewKey[] = "comm_view";
constexpr char kModeKey[] = "comm_radio";

std::uint8_t load_validated(const char* key, std::uint8_t max_value)
{
    nvs_handle_t handle = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &handle) != ESP_OK) return 0U;
    std::uint8_t value = 0U;
    const esp_err_t result = nvs_get_u8(handle, key, &value);
    nvs_close(handle);
    if (result != ESP_OK || value > max_value) {
        if (result != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(kTag, "Invalid/unreadable preference: %s", key);
        }
        return 0U;
    }
    return value;
}

bool save_validated(const char* key, std::uint8_t value, std::uint8_t max_value)
{
    if (value > max_value) return false;
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result == ESP_OK) result = nvs_set_u8(handle, key, value);
    if (result == ESP_OK) result = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "Failed to store preference %s: %s",
            key, esp_err_to_name(result));
        return false;
    }
    return true;
}
}  // namespace

namespace nikos::storage {
std::uint8_t CommunicatorPreferencesStore::load_view() const
{
    return load_validated(kViewKey, 2U);
}
std::uint8_t CommunicatorPreferencesStore::load_radio_mode() const
{
    return load_validated(kModeKey, 1U);
}
bool CommunicatorPreferencesStore::save_view(std::uint8_t view) const
{
    return save_validated(kViewKey, view, 2U);
}
bool CommunicatorPreferencesStore::save_radio_mode(std::uint8_t mode) const
{
    return save_validated(kModeKey, mode, 1U);
}
}  // namespace nikos::storage
