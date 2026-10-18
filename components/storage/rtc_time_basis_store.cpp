#include "storage/rtc_time_basis_store.hpp"
#include "nvs.h"
namespace nikos::storage {
namespace {
constexpr char kNamespace[] = "nikos";
constexpr char kKey[] = "rtc_basis";
}
RtcTimeBasis RtcTimeBasisStore::load() const {
    nvs_handle_t h=0;
    const auto open=nvs_open(kNamespace,NVS_READONLY,&h);
    if (open==ESP_ERR_NVS_NOT_FOUND) return RtcTimeBasis::LegacyLocal;
    if (open!=ESP_OK) return RtcTimeBasis::Uncertain;
    std::uint8_t value=0;
    const auto err=nvs_get_u8(h,kKey,&value);
    nvs_close(h);
    if (err==ESP_ERR_NVS_NOT_FOUND) return RtcTimeBasis::LegacyLocal;
    if (err!=ESP_OK) return RtcTimeBasis::Uncertain;
    if (value==1) return RtcTimeBasis::Pending;
    if (value==2) return RtcTimeBasis::Utc;
    return RtcTimeBasis::Uncertain;
}
bool RtcTimeBasisStore::save(RtcTimeBasis basis) const {
    if (basis!=RtcTimeBasis::Pending && basis!=RtcTimeBasis::Utc) return false;
    nvs_handle_t h=0;
    if (nvs_open(kNamespace,NVS_READWRITE,&h)!=ESP_OK) return false;
    auto err=nvs_set_u8(h,kKey,basis==RtcTimeBasis::Utc?2U:1U);
    if (err==ESP_OK) err=nvs_commit(h);
    nvs_close(h);
    return err==ESP_OK;
}
} // namespace nikos::storage
