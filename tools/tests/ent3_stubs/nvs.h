#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

using esp_err_t = int;
using nvs_handle_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_NVS_NOT_FOUND = 0x1102;
constexpr esp_err_t ESP_ERR_NVS_INVALID_LENGTH = 0x110c;
constexpr int NVS_READONLY = 1;
constexpr int NVS_READWRITE = 2;

namespace fake_nvs {
inline std::vector<std::uint8_t> durable;
inline std::vector<std::uint8_t> pending;
inline bool exists = false;
inline bool pending_exists = false;
inline bool pending_dirty = false;
inline bool fail_open = false;
inline bool fail_get = false;
inline bool fail_set = false;
inline bool fail_erase = false;
inline bool fail_commit = false;
inline void reset() {
    durable.clear();
    pending.clear();
    exists = false;
    pending_exists = false;
    pending_dirty = false;
    fail_open = fail_get = fail_set = fail_erase = fail_commit = false;
}
}
inline esp_err_t nvs_open(const char*, int, nvs_handle_t* handle) {
    if (fake_nvs::fail_open) return ESP_FAIL;
    *handle = 1;
    return ESP_OK;
}
inline void nvs_close(nvs_handle_t) {}
inline esp_err_t nvs_get_blob(nvs_handle_t, const char*,
                              void* result, std::size_t* size) {
    if (fake_nvs::fail_get) return ESP_FAIL;
    if (!fake_nvs::exists) return ESP_ERR_NVS_NOT_FOUND;
    if (!result) {
        *size = fake_nvs::durable.size();
        return ESP_OK;
    }
    if (*size < fake_nvs::durable.size()) return ESP_ERR_NVS_INVALID_LENGTH;
    *size = fake_nvs::durable.size();
    std::memcpy(result, fake_nvs::durable.data(), *size);
    return ESP_OK;
}
inline esp_err_t nvs_set_blob(nvs_handle_t, const char*,
                              const void* data, std::size_t size) {
    if (fake_nvs::fail_set) return ESP_FAIL;
    const auto* p = static_cast<const std::uint8_t*>(data);
    fake_nvs::pending.assign(p, p + size);
    fake_nvs::pending_exists = true;
    fake_nvs::pending_dirty = true;
    return ESP_OK;
}
inline esp_err_t nvs_erase_key(nvs_handle_t, const char*) {
    if (fake_nvs::fail_erase) return ESP_FAIL;
    if (!fake_nvs::exists) return ESP_ERR_NVS_NOT_FOUND;
    fake_nvs::pending_exists = false;
    fake_nvs::pending_dirty = true;
    return ESP_OK;
}
inline esp_err_t nvs_commit(nvs_handle_t) {
    if (fake_nvs::fail_commit) return ESP_FAIL;
    if (fake_nvs::pending_dirty) {
        fake_nvs::exists = fake_nvs::pending_exists;
        fake_nvs::durable = fake_nvs::pending;
        fake_nvs::pending_dirty = false;
    }
    return ESP_OK;
}
