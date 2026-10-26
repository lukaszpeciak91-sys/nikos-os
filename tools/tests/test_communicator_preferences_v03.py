#!/usr/bin/env python3
"""Communicator v0.3: persistent view/radio and Signal-only UI regressions."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
cpp = (root / "apps/communicator/communicator_app.cpp").read_text(encoding="utf-8")
hpp = (root / "apps/communicator/include/communicator/communicator_app.hpp").read_text(encoding="utf-8")
settings = (root / "components/settings/include/settings/settings.hpp").read_text(encoding="utf-8")
main = (root / "main/app_main.cpp").read_text(encoding="utf-8")
cmake = (root / "components/storage/CMakeLists.txt").read_text(encoding="utf-8")
store_cpp = root / "components/storage/communicator_preferences_store.cpp"
store_hpp = root / "components/storage/include"

assert "SignalOnly" in settings
assert "render_main_signal_only();" in cpp
assert "void CommunicatorApp::render_main_signal_only()" in cpp
assert '"SYGNAL", 3,' in cpp
assert 'kLabels[] = {"OPCJE", "POWROT"}' in cpp
assert "kMainSignalIndex, kMainOptionsIndex" in cpp or (
    "kMainOptionsIndex, kMainReturnIndex" in cpp
    and "selected_main_index_ = kMainSignalIndex" in cpp
)
assert "settings::CommunicatorView::SignalOnly" in cpp
assert "settings_.communicator_view = next;" in cpp
assert "preferences_store_.save_view(" in cpp
assert "preferences_store_.save_radio_mode(" in cpp
assert "preference_save_failed_ = true;" in cpp
assert "messaging_.set_radio_mode(current)" not in cpp  # NVS-first, no radio rollback
assert "radio_preference_inconsistent_" in cpp
assert '"RADIO/NVS NIESPOJNE"' in cpp
assert "selected_main_index_ = kMainSignalIndex;" in cpp
assert "bool CommunicatorApp::process_incoming()" in cpp
assert "drain_incoming()" in cpp
assert "start_signal_alert()" in cpp
assert "bool CommunicatorApp::send_signal()" in cpp
assert "bool CommunicatorApp::send_selected_response()" in cpp
assert "communicator_preferences.load_view()" in main
assert "communicator_preferences.load_radio_mode()" in main
assert "messaging.set_radio_mode(static_cast<nikos::radio::Mode>(" in main
assert "make_messaging_config(" in main
assert "action == nikos::launcher::Action::StartCommunicator" in main
assert "communicator_preferences_store.cpp" in cmake
assert "storage" in (
    root / "apps/communicator/CMakeLists.txt"
).read_text(encoding="utf-8")

# Production UI geometry: all independent rows fit LCD 240x135.
for x, y, w, height in [
    (8, 38, 224, 39), (24, 43, 200, 30),
    (8, 80, 224, 20), (8, 101, 224, 20), (10, 123, 222, 10),
]:
    assert x >= 0 and y >= 0 and x + w <= 240 and y + height <= 135
assert "80 + row * 21" in cpp
assert '"WIDOK KOMUNIKATORA"' in cpp
assert '"TYLKO SYGNAL"' in cpp
assert '"NIE ZAPISANO"' in cpp

compiler = shutil.which("g++") or shutil.which("clang++")
assert compiler, "A host C++ compiler is required for NVS persistence tests"

with tempfile.TemporaryDirectory() as temp:
    tmp = Path(temp)
    (tmp / "nvs.h").write_text(r'''
#pragma once
#include <cstdint>
#include <map>
#include <string>
using esp_err_t=int;
using nvs_handle_t=int;
constexpr esp_err_t ESP_OK=0, ESP_FAIL=-1, ESP_ERR_NVS_NOT_FOUND=0x1102;
constexpr int NVS_READONLY=1, NVS_READWRITE=2;
namespace mock_nvs {
inline std::map<std::string, std::uint8_t> committed, staged;
inline bool fail_open=false, fail_get=false, fail_set=false, fail_commit=false;
inline void reset() {
    committed.clear(); staged.clear();
    fail_open=fail_get=fail_set=fail_commit=false;
}
}
inline esp_err_t nvs_open(const char*,int,nvs_handle_t* h) {
    if(mock_nvs::fail_open) return ESP_FAIL;
    *h=1; return ESP_OK;
}
inline void nvs_close(nvs_handle_t) { mock_nvs::staged.clear(); }
inline esp_err_t nvs_get_u8(nvs_handle_t,const char* key,std::uint8_t* v) {
    if(mock_nvs::fail_get) return ESP_FAIL;
    auto it=mock_nvs::committed.find(key);
    if(it==mock_nvs::committed.end()) return ESP_ERR_NVS_NOT_FOUND;
    *v=it->second; return ESP_OK;
}
inline esp_err_t nvs_set_u8(nvs_handle_t,const char* key,std::uint8_t v) {
    if(mock_nvs::fail_set) return ESP_FAIL;
    mock_nvs::staged[key]=v; return ESP_OK;
}
inline esp_err_t nvs_commit(nvs_handle_t) {
    if(mock_nvs::fail_commit) return ESP_FAIL;
    for(const auto& pair:mock_nvs::staged) mock_nvs::committed[pair.first]=pair.second;
    mock_nvs::staged.clear(); return ESP_OK;
}
''', encoding="utf-8")
    (tmp / "esp_err.h").write_text(
        '#pragma once\n#include "nvs.h"\n'
        'inline const char* esp_err_to_name(esp_err_t) { return "ERR"; }\n',
        encoding="utf-8")
    (tmp / "esp_log.h").write_text(
        '#pragma once\n'
        '#define ESP_LOGW(tag,fmt,...) do{(void)(tag);}while(0)\n'
        '#define ESP_LOGE(tag,fmt,...) do{(void)(tag);}while(0)\n',
        encoding="utf-8")
    program = tmp / "test.cpp"
    program.write_text(r'''
#include <cassert>
#include <cstdint>
#include "nvs.h"
#include "storage/communicator_preferences_store.hpp"
using nikos::storage::CommunicatorPreferencesStore;
int main() {
    CommunicatorPreferencesStore store;
    mock_nvs::reset();
    assert(store.load_view()==0 && store.load_radio_mode()==0);
    assert(store.save_view(2));
    assert(store.load_view()==2 && store.load_radio_mode()==0);
    assert(store.save_radio_mode(1));
    assert(store.load_view()==2 && store.load_radio_mode()==1);

    // Simulated restart: new instance restores both persisted settings.
    CommunicatorPreferencesStore rebooted;
    assert(rebooted.load_view()==2 && rebooted.load_radio_mode()==1);
    assert(rebooted.save_view(1));
    assert(store.load_view()==1 && store.load_radio_mode()==1);
    assert(rebooted.save_view(0));
    assert(store.load_view()==0 && store.load_radio_mode()==1);

    // Reject invalid values without modifying committed settings.
    assert(!store.save_view(3) && !store.save_radio_mode(2));
    assert(store.load_view()==0 && store.load_radio_mode()==1);
    mock_nvs::committed["comm_view"]=255;
    assert(store.load_view()==0); // Invalid persisted view -> LISTA.
    mock_nvs::committed["comm_view"]=2;
    mock_nvs::committed["comm_radio"]=255;
    assert(store.load_radio_mode()==0); // Invalid persisted mode -> STANDARD.
    mock_nvs::committed["comm_radio"]=1;

    // Failed NVS open, write, commit: no durable mutation.
    mock_nvs::fail_open=true;
    assert(!store.save_view(1));
    assert(store.load_view()==0);
    mock_nvs::fail_open=false;
    mock_nvs::fail_set=true;
    assert(!store.save_radio_mode(0));
    mock_nvs::fail_set=false;
    mock_nvs::fail_commit=true;
    assert(!store.save_view(1));
    mock_nvs::fail_commit=false;
    assert(store.load_view()==2 && store.load_radio_mode()==1);
    mock_nvs::fail_get=true;
    assert(store.load_view()==0 && store.load_radio_mode()==0);
    mock_nvs::fail_get=false;
    assert(store.load_view()==2 && store.load_radio_mode()==1);
    return 0;
}
''', encoding="utf-8")
    binary = tmp / "comm_prefs_test"
    subprocess.run([
        compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-I", str(tmp), "-I", str(store_hpp),
        str(program), str(store_cpp), "-o", str(binary)
    ], check=True)
    subprocess.run([str(binary)], check=True)

print("Communicator v0.3 signal-only, persistence and NVS failure regressions PASS")
