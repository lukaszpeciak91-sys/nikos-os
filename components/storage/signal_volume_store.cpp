#include "storage/signal_volume_store.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"
namespace {
constexpr char kTag[] = "signal_volume";
constexpr char kNamespace[] = "nikos";
constexpr char kKey[] = "sig_vol";
}
namespace nikos::storage {
std::uint8_t SignalVolumeStore::load_level() const {
    nvs_handle_t h = 0;
    if (nvs_open(kNamespace, NVS_READONLY, &h) != ESP_OK) return 4;
    std::uint8_t level = 4;
    const esp_err_t err = nvs_get_u8(h, kKey, &level);
    nvs_close(h);
    if (err != ESP_OK || level > 4) return 4;
    return level;
}
bool SignalVolumeStore::save_level(std::uint8_t level) const {
    if (level > 4) return false;
    nvs_handle_t h = 0;
    esp_err_t err = nvs_open(kNamespace, NVS_READWRITE, &h);
    if (err == ESP_OK) err = nvs_set_u8(h, kKey, level);
    if (err == ESP_OK) err = nvs_commit(h);
    if (h != 0) nvs_close(h);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "Volume save failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}
}
