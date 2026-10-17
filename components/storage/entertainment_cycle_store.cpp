#include "storage/entertainment_cycle_store.hpp"
#include "nvs.h"
#include "esp_log.h"
namespace nikos::storage {
namespace {
constexpr char kNamespace[]="nikos";
constexpr char kKey[]="ent_cycle";
struct Record {
    std::uint32_t marker;
    std::uint32_t civil_start;
    std::uint8_t civil_valid;
    std::uint8_t reserved[3]{};
};
constexpr std::uint32_t kMarker=0x454E5431U;
bool write(const Record* record) {
    nvs_handle_t handle=0;
    esp_err_t err=nvs_open(kNamespace,NVS_READWRITE,&handle);
    if(err!=ESP_OK) { ESP_LOGW("ent_cycle","NVS open failed: %d",err);return false; }
    if(record)err=nvs_set_blob(handle,kKey,record,sizeof(*record));
    else err=nvs_erase_key(handle,kKey);
    if(err==ESP_ERR_NVS_NOT_FOUND)err=ESP_OK;
    if(err==ESP_OK)err=nvs_commit(handle);
    nvs_close(handle);
    if(err!=ESP_OK)ESP_LOGW("ent_cycle","NVS write failed: %d",err);
    return err==ESP_OK;
}
}
bool EntertainmentCycleStore::load(EntertainmentCycle& cycle) const {
    cycle={};
    nvs_handle_t handle=0;
    esp_err_t err=nvs_open(kNamespace,NVS_READONLY,&handle);
    if(err==ESP_ERR_NVS_NOT_FOUND)return true;
    if(err!=ESP_OK)return false;
    Record record{};
    std::size_t size=sizeof(record);
    err=nvs_get_blob(handle,kKey,&record,&size);
    nvs_close(handle);
    if(err==ESP_ERR_NVS_NOT_FOUND)return true;
    if(err!=ESP_OK || size!=sizeof(record) || record.marker!=kMarker ||
        record.civil_valid>1U)return false;
    cycle.active=true;
    cycle.civil_valid=record.civil_valid!=0U;
    cycle.civil_start=record.civil_start;
    return true;
}
bool EntertainmentCycleStore::save(const EntertainmentCycle& cycle) const {
    if(!cycle.active)return clear();
    const Record record{kMarker,cycle.civil_start,
                        static_cast<std::uint8_t>(cycle.civil_valid?1U:0U),{}};
    return write(&record);
}
bool EntertainmentCycleStore::clear() const {return write(nullptr);}
}
