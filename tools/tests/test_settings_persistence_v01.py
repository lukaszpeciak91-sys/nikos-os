#!/usr/bin/env python3
"""Execute production Launcher preference confirmations and NVS persistence."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[2]
SRC=(ROOT/"components/launcher/launcher.cpp").read_text(encoding="utf-8")
APP=(ROOT/"main/app_main.cpp").read_text(encoding="utf-8")
assert "storage::UiPreferencesStore ui_preferences_store_" in (ROOT/"components/launcher/include/launcher/launcher.hpp").read_text()
assert APP.index("if (!initialize_nvs())") < APP.index("settings.theme = ui_preferences_store.load_theme()")
assert APP.index("settings.signal_sound = ui_preferences_store.load_signal_sound()") < APP.index("launcher.begin(communicator_status")
assert "battery_guard.low_battery_reserve_active()" in APP
assert "bool communicator_enabled = false;" in APP
for text in ("signal_volume_store.load_level()", "auto_power_off_store.load_enabled()",
             "communicator_preferences.load_view()", "communicator_preferences.load_radio_mode()"):
    assert text in APP

def balanced(source, needle):
    start=source.index(needle)
    begin=source.index("{", start)
    depth=0
    for i in range(begin,len(source)):
        if source[i]=="{": depth+=1
        elif source[i]=="}":
            depth-=1
            if depth==0: return source[start:i+1]
    raise AssertionError(needle)

update=SRC.split("Action Launcher::update(",1)[1]
blocks="\n".join(balanced(update,"if (screen_ == Screen::"+name+") {")
                 for name in ("SignalSound","Brightness","Theme","Orientation"))
helpers=SRC[SRC.index("std::uint8_t signal_sound_index("):SRC.index("\nvoid format_mmss(")]
a=SRC.index("std::uint8_t theme_index(")
b=SRC.index("\n}",SRC.index("nikos::ui_theme::Theme theme_from_index(",a))+2
helpers+="\n"+SRC[a:b]

nvs_stub=r"""
#pragma once
#include <cstdint>
#include <map>
#include <string>
using esp_err_t=int;using nvs_handle_t=int;
constexpr esp_err_t ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NVS_NOT_FOUND=0x1102;
constexpr int NVS_READONLY=1,NVS_READWRITE=2;
namespace mock {
inline std::map<std::string,std::uint8_t> stored,pending;
inline bool fail_open=false,fail_get=false,fail_set=false,fail_commit=false;
inline int writes=0;
inline void reset(){stored.clear();pending.clear();fail_open=fail_get=fail_set=fail_commit=false;writes=0;}
}
inline esp_err_t nvs_open(const char*,int,nvs_handle_t* h){if(mock::fail_open)return ESP_FAIL;*h=1;return ESP_OK;}
inline void nvs_close(nvs_handle_t){mock::pending.clear();}
inline esp_err_t nvs_get_u8(nvs_handle_t,const char* k,std::uint8_t* v){
 if(mock::fail_get)return ESP_FAIL;
 auto it=mock::stored.find(k);
 if(it==mock::stored.end())return ESP_ERR_NVS_NOT_FOUND;
 *v=it->second;return ESP_OK;
}
inline esp_err_t nvs_set_u8(nvs_handle_t,const char* k,std::uint8_t v){
 ++mock::writes;if(mock::fail_set)return ESP_FAIL;
 mock::pending[k]=v;return ESP_OK;
}
inline esp_err_t nvs_commit(nvs_handle_t){
 if(mock::fail_commit)return ESP_FAIL;
 for(auto const& p:mock::pending)mock::stored[p.first]=p.second;
 mock::pending.clear();return ESP_OK;
}
"""
store_test=r"""
#include <cassert>
#include "nvs.h"
#include "storage/ui_preferences_store.hpp"
using namespace nikos;
int main(){
 mock::reset();storage::UiPreferencesStore s;
 assert(s.load_theme()==ui_theme::Theme::Nikos);
 assert(s.load_orientation()==settings::Orientation::Right);
 assert(s.load_brightness()==settings::Brightness::Medium);
 assert(s.load_signal_sound()==settings::SignalSound::Gentle);
 for(int i=0;i<5;i++){assert(s.save_theme(static_cast<ui_theme::Theme>(i)));assert(s.load_theme()==static_cast<ui_theme::Theme>(i));}
 for(int i=0;i<2;i++){assert(s.save_orientation(static_cast<settings::Orientation>(i)));assert(s.load_orientation()==static_cast<settings::Orientation>(i));}
 for(int i=0;i<3;i++){
  assert(s.save_brightness(static_cast<settings::Brightness>(i)));
  assert(s.load_brightness()==static_cast<settings::Brightness>(i));
  assert(s.save_signal_sound(static_cast<settings::SignalSound>(i)));
  assert(s.load_signal_sound()==static_cast<settings::SignalSound>(i));
 }
 storage::UiPreferencesStore reboot;
 assert(reboot.load_theme()==ui_theme::Theme::Noir);
 assert(reboot.load_orientation()==settings::Orientation::Left);
 assert(reboot.load_brightness()==settings::Brightness::High);
 assert(reboot.load_signal_sound()==settings::SignalSound::Pager);
 mock::stored["ui_theme"]=255;
 assert(s.load_theme()==ui_theme::Theme::Nikos);
 assert(s.load_orientation()==settings::Orientation::Left);
 mock::stored["ui_theme"]=4;mock::stored["ui_orient"]=255;
 assert(s.load_orientation()==settings::Orientation::Right);
 assert(s.load_theme()==ui_theme::Theme::Noir);
 mock::stored["ui_orient"]=1;mock::stored["ui_bright"]=255;
 assert(s.load_brightness()==settings::Brightness::Medium);
 assert(s.load_signal_sound()==settings::SignalSound::Pager);
 mock::stored["ui_bright"]=2;mock::stored["sig_tune"]=255;
 assert(s.load_signal_sound()==settings::SignalSound::Gentle);
 assert(s.load_brightness()==settings::Brightness::High);
 mock::stored["sig_tune"]=2;
 assert(!s.save_theme(static_cast<ui_theme::Theme>(5)));
 assert(!s.save_orientation(static_cast<settings::Orientation>(2)));
 assert(!s.save_brightness(static_cast<settings::Brightness>(3)));
 assert(!s.save_signal_sound(static_cast<settings::SignalSound>(3)));
 for(int mode=0;mode<4;mode++){
  mock::fail_open=mode==0;mock::fail_get=mode==1;
  if(mode<2){
   assert(s.load_theme()==ui_theme::Theme::Nikos);
   assert(s.load_orientation()==settings::Orientation::Right);
   assert(s.load_brightness()==settings::Brightness::Medium);
   assert(s.load_signal_sound()==settings::SignalSound::Gentle);
  }
  mock::fail_get=false;mock::fail_set=mode==2;mock::fail_commit=mode==3;
  assert(!s.save_theme(ui_theme::Theme::Matrix));
  assert(!s.save_orientation(settings::Orientation::Right));
  assert(!s.save_brightness(settings::Brightness::Low));
  assert(!s.save_signal_sound(settings::SignalSound::Classic));
  mock::fail_open=mock::fail_set=mock::fail_commit=false;
  assert(s.load_theme()==ui_theme::Theme::Noir);
  assert(s.load_orientation()==settings::Orientation::Left);
  assert(s.load_brightness()==settings::Brightness::High);
  assert(s.load_signal_sound()==settings::SignalSound::Pager);
 }
 return 0;
}
"""
launcher_head=r"""
#include <cassert>
#include <cstdint>
#include "settings/settings.hpp"
namespace nikos::board{
 enum class DisplayOrientation{Right,Left};
 struct InputState{bool secondary_long=false,secondary_short=false,primary_short=false;};
 struct Board{
  ui_theme::Theme theme=ui_theme::Theme::Nikos;DisplayOrientation orientation=DisplayOrientation::Right;
  int theme_calls=0,orient_calls=0;
  void set_theme(ui_theme::Theme v){theme=v;++theme_calls;}
  void set_display_orientation(DisplayOrientation v){orientation=v;++orient_calls;}
 };
}
namespace nikos::signal_sound{
 struct Player{int stops=0,plays=0;void stop(){++stops;}void play_selected(){++plays;}};
}
namespace nikos::launcher{
enum class Action{None,BrightnessChanged};
enum class Screen{Settings,SoundMenu,SignalSound,Brightness,Theme,Orientation};
struct FakeStore{
 bool fail=false;int writes=0;
 bool save_theme(ui_theme::Theme){++writes;return !fail;}
 bool save_orientation(settings::Orientation){++writes;return !fail;}
 bool save_brightness(settings::Brightness){++writes;return !fail;}
 bool save_signal_sound(settings::SignalSound){++writes;return !fail;}
};
struct Harness{
 settings::State& settings_;board::Board& board_;signal_sound::Player& signal_sound_;
 FakeStore& ui_preferences_store_;
 Screen screen_=Screen::Settings;
 std::uint8_t signal_sound_selection_=0,sound_menu_selection_=0,
 settings_selection_=0,brightness_selection_=0,theme_selection_=0,orientation_selection_=0;
 bool settings_save_failed_=false;
 int renders=0;void render(){++renders;}
 Action update(const board::InputState&);
};
}
"""
launcher_test=r"""
using namespace nikos;
int main(){
 settings::State settings;
 board::Board board;
 signal_sound::Player player;
 launcher::FakeStore store;
 launcher::Harness ui{settings,board,player,store};
 board::InputState m5{};m5.primary_short=true;
 board::InputState side{};side.secondary_short=true;
 ui.screen_=launcher::Screen::Theme;ui.theme_selection_=4;
 assert(ui.update(m5)==launcher::Action::None);
 assert(settings.theme==ui_theme::Theme::Noir && board.theme==ui_theme::Theme::Noir);
 int calls=store.writes;ui.update(m5);assert(store.writes==calls);
 store.fail=true;ui.theme_selection_=2;ui.update(m5);
 assert(settings.theme==ui_theme::Theme::Noir && ui.theme_selection_==4 && ui.settings_save_failed_);
 assert(board.theme==ui_theme::Theme::Noir);
 ui.update(side);assert(!ui.settings_save_failed_);
 store.fail=false;
 ui.screen_=launcher::Screen::Orientation;ui.orientation_selection_=1;
 ui.update(m5);assert(settings.orientation==settings::Orientation::Left);
 assert(board.orientation==board::DisplayOrientation::Left);
 calls=store.writes;ui.update(m5);assert(store.writes==calls);
 store.fail=true;ui.orientation_selection_=0;ui.update(m5);
 assert(settings.orientation==settings::Orientation::Left);
 assert(ui.orientation_selection_==1 && ui.settings_save_failed_);
 assert(board.orientation==board::DisplayOrientation::Left);
 store.fail=false;
 ui.screen_=launcher::Screen::Brightness;ui.brightness_selection_=0;
 assert(ui.update(m5)==launcher::Action::BrightnessChanged);
 assert(settings.brightness==settings::Brightness::Low);
 calls=store.writes;assert(ui.update(m5)==launcher::Action::None && store.writes==calls);
 store.fail=true;ui.brightness_selection_=2;
 assert(ui.update(m5)==launcher::Action::None);
 assert(settings.brightness==settings::Brightness::Low);
 assert(ui.brightness_selection_==0 && ui.settings_save_failed_);
 store.fail=false;ui.brightness_selection_=2;ui.update(m5);
 assert(settings.brightness==settings::Brightness::High);
 ui.screen_=launcher::Screen::SignalSound;ui.signal_sound_selection_=2;
 int prev=player.plays;ui.update(m5);
 assert(settings.signal_sound==settings::SignalSound::Pager && player.plays==prev+1);
 calls=store.writes;ui.update(m5);
 assert(store.writes==calls && player.plays==prev+2);
 store.fail=true;ui.signal_sound_selection_=1;ui.update(m5);
 assert(settings.signal_sound==settings::SignalSound::Pager);
 assert(ui.signal_sound_selection_==2 && ui.settings_save_failed_);
 assert(player.plays==prev+2);
 const auto low=settings::effective_brightness_profile(settings.brightness,true);
 assert(low.active==72 && low.dimmed==18);
 const auto high=settings::effective_brightness_profile(settings.brightness,false);
 assert(high.active==128 && high.dimmed==32);
 return 0;
}
"""
compiler=shutil.which("g++") or shutil.which("clang++")
assert compiler
with tempfile.TemporaryDirectory() as td:
    tmp=Path(td)
    (tmp/"nvs.h").write_text(nvs_stub,encoding="utf-8")
    (tmp/"esp_err.h").write_text('#pragma once\n#include "nvs.h"\ninline const char* esp_err_to_name(esp_err_t){return "err";}\n')
    (tmp/"esp_log.h").write_text('#pragma once\n#define ESP_LOGW(tag,...) do{(void)(tag);}while(0)\n#define ESP_LOGE(tag,...) do{(void)(tag);}while(0)\n')
    includes=["-I"+str(tmp),"-I"+str(ROOT/"components/storage/include"),
              "-I"+str(ROOT/"components/settings/include"),
              "-I"+str(ROOT/"components/ui_theme/include")]
    args=[compiler,"-std=c++17","-Wall","-Wextra","-Werror",*includes]
    (tmp/"store_test.cpp").write_text(store_test,encoding="utf-8")
    subprocess.run([*args,str(tmp/"store_test.cpp"),str(ROOT/"components/storage/ui_preferences_store.cpp"),"-o",str(tmp/"store")],check=True)
    subprocess.run([str(tmp/"store")],check=True)
    handlers=launcher_head+"\n"+helpers+"\nnamespace nikos::launcher {\nAction Harness::update(const board::InputState& input){\n"+blocks+"\nreturn Action::None;\n}\n}\n"+launcher_test
    (tmp/"launcher_test.cpp").write_text(handlers,encoding="utf-8")
    subprocess.run([*args,str(tmp/"launcher_test.cpp"),str(ROOT/"components/settings/settings.cpp"),"-o",str(tmp/"launcher")],check=True)
    subprocess.run([str(tmp/"launcher")],check=True)
print("UI preference NVS, M5 production handlers, rollback and reserve PASS")
