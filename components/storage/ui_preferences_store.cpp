#include "storage/ui_preferences_store.hpp"

#include <cstdint>

#include "esp_err.h"
#include "esp_log.h"
#include "nvs.h"

namespace {
constexpr char kTag[] = "ui_prefs";
constexpr char kNamespace[] = "nikos";
// Stable u8 representations, independent of any C++ struct layout.
constexpr char kThemeKey[] = "ui_theme";   // 0..4
constexpr char kOrientKey[] = "ui_orient";  // 0..1
constexpr char kBrightKey[] = "ui_bright";  // 0..2; default 1
constexpr char kMelodyKey[] = "sig_tune";   // 0..2

// Guard the NVS wire values against accidental enum reordering.
static_assert(static_cast<std::uint8_t>(nikos::ui_theme::Theme::Nikos) == 0U);
static_assert(static_cast<std::uint8_t>(nikos::ui_theme::Theme::Bursztyn) == 1U);
static_assert(static_cast<std::uint8_t>(nikos::ui_theme::Theme::Matrix) == 2U);
static_assert(static_cast<std::uint8_t>(nikos::ui_theme::Theme::Lava) == 3U);
static_assert(static_cast<std::uint8_t>(nikos::ui_theme::Theme::Noir) == 4U);
static_assert(static_cast<std::uint8_t>(nikos::settings::Orientation::Right) == 0U);
static_assert(static_cast<std::uint8_t>(nikos::settings::Orientation::Left) == 1U);
static_assert(static_cast<std::uint8_t>(nikos::settings::Brightness::Low) == 0U);
static_assert(static_cast<std::uint8_t>(nikos::settings::Brightness::Medium) == 1U);
static_assert(static_cast<std::uint8_t>(nikos::settings::Brightness::High) == 2U);
static_assert(static_cast<std::uint8_t>(nikos::settings::SignalSound::Gentle) == 0U);
static_assert(static_cast<std::uint8_t>(nikos::settings::SignalSound::Classic) == 1U);
static_assert(static_cast<std::uint8_t>(nikos::settings::SignalSound::Pager) == 2U);


std::uint8_t load(const char* key, std::uint8_t fallback, std::uint8_t maximum)
{
    nvs_handle_t handle = 0;
    const esp_err_t opened = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (opened != ESP_OK) {
        ESP_LOGW(kTag, "NVS open failed for %s", key);
        return fallback;
    }
    std::uint8_t value = fallback;
    const esp_err_t result = nvs_get_u8(handle, key, &value);
    nvs_close(handle);
    if (result == ESP_OK && value <= maximum) return value;
    if (result != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(kTag, "Invalid or unreadable NVS preference %s", key);
    }
    return fallback;
}

bool save(const char* key, std::uint8_t value, std::uint8_t maximum)
{
    if (value > maximum) return false;
    nvs_handle_t handle = 0;
    esp_err_t result = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (result == ESP_OK) result = nvs_set_u8(handle, key, value);
    if (result == ESP_OK) result = nvs_commit(handle);
    if (handle != 0) nvs_close(handle);
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "Preference save failed for %s: %s",
            key, esp_err_to_name(result));
        return false;
    }
    return true;
}
}  // namespace

namespace nikos::storage {
ui_theme::Theme UiPreferencesStore::load_theme() const
{
    return static_cast<ui_theme::Theme>(load(kThemeKey, 0U, 4U));
}
settings::Orientation UiPreferencesStore::load_orientation() const
{
    return static_cast<settings::Orientation>(load(kOrientKey, 0U, 1U));
}
settings::Brightness UiPreferencesStore::load_brightness() const
{
    return static_cast<settings::Brightness>(load(kBrightKey, 1U, 2U));
}
settings::SignalSound UiPreferencesStore::load_signal_sound() const
{
    return static_cast<settings::SignalSound>(load(kMelodyKey, 0U, 2U));
}
bool UiPreferencesStore::save_theme(ui_theme::Theme value) const
{
    return save(kThemeKey, static_cast<std::uint8_t>(value), 4U);
}
bool UiPreferencesStore::save_orientation(settings::Orientation value) const
{
    return save(kOrientKey, static_cast<std::uint8_t>(value), 1U);
}
bool UiPreferencesStore::save_brightness(settings::Brightness value) const
{
    return save(kBrightKey, static_cast<std::uint8_t>(value), 2U);
}
bool UiPreferencesStore::save_signal_sound(settings::SignalSound value) const
{
    return save(kMelodyKey, static_cast<std::uint8_t>(value), 2U);
}
}  // namespace nikos::storage
