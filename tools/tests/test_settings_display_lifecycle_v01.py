#!/usr/bin/env python3
"""Execute real DisplayLifecycle with restored user brightness and Reserve limits."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
compiler=shutil.which("g++") or shutil.which("clang++")
assert compiler
main=(root/"main/app_main.cpp").read_text(encoding="utf-8")
assert "settings.brightness = ui_preferences_store.load_brightness();" in main
assert "apply_effective_brightness_profile(" in main
assert "battery_guard.low_battery_reserve_active()" in main
assert "display_lifecycle.display_off_now();" in main
assert "show_charging_presentation(" in main
assert "prepare_flashlight_for_foreground_takeover(" in main
assert "clock_glance_active = true;" in main

board=r"""
#pragma once
#include <cstdint>
namespace nikos::board {
struct InputState {
 bool primary_short=false,primary_long=false,secondary_short=false,secondary_long=false;
 bool primary_pressed=false,secondary_pressed=false,power_short=false;
};
struct Board {
 enum class Level{Active,Dimmed,Off};
 std::uint8_t active=96,dimmed=24,current=96;
 Level level=Level::Active;
 void set_display_brightness_profile(std::uint8_t a,std::uint8_t d){active=a;dimmed=d;}
 void apply_active_display_brightness(){level=Level::Active;current=active;}
 void wake_display(){apply_active_display_brightness();}
 void dim_display(){level=Level::Dimmed;current=dimmed;}
 void sleep_display(){level=Level::Off;current=0;}
};
}
"""
program=r"""
#include <cassert>
#include <cstdint>
#include "board/board.hpp"
#include "power/display_lifecycle.hpp"
#include "settings/settings.hpp"
std::int64_t fake_us=0;
int main(){
 using namespace nikos;
 board::Board board;
 power::DisplayLifecycle lifecycle(board);
 settings::State saved;
 saved.brightness=settings::Brightness::High;
 lifecycle.begin();
 auto preferred=settings::effective_brightness_profile(saved.brightness,false);
 lifecycle.apply_brightness_profile(preferred.active,preferred.dimmed);
 assert(board.current==128 && lifecycle.state()==power::DisplayState::Active);
 fake_us=15000000;
 lifecycle.update();
 assert(board.current==32 && lifecycle.state()==power::DisplayState::Dimmed);
 auto reserve=settings::effective_brightness_profile(saved.brightness,true);
 lifecycle.apply_brightness_profile(reserve.active,reserve.dimmed);
 assert(board.current==18 && board.active==72 && board.dimmed==18);
 assert(saved.brightness==settings::Brightness::High);
 fake_us=45000000;
 lifecycle.update();
 assert(board.current==0 && lifecycle.state()==power::DisplayState::DisplayOff);
 // Restoring preference while physically asleep must not light the screen.
 lifecycle.apply_brightness_profile(preferred.active,preferred.dimmed);
 assert(board.current==0 && lifecycle.state()==power::DisplayState::DisplayOff);
 board::InputState wake{};wake.power_short=true;
 auto filtered=lifecycle.filter_input(wake);
 assert(filtered.wake_reason==power::WakeReason::UserButton);
 assert(board.current==128 && lifecycle.state()==power::DisplayState::Active);
 // Low battery clamps user profile without rewriting the saved setting.
 lifecycle.apply_brightness_profile(reserve.active,reserve.dimmed);
 assert(board.current==72 && saved.brightness==settings::Brightness::High);
 fake_us=46000000;
 auto off=lifecycle.filter_input(wake);
 assert(off.power_display_off && board.current==0);
 lifecycle.apply_brightness_profile(reserve.active,reserve.dimmed);
 assert(board.current==0);
 auto waking=lifecycle.filter_input(wake);
 assert(waking.wake_reason==power::WakeReason::UserButton && board.current==72);
 lifecycle.apply_brightness_profile(preferred.active,preferred.dimmed);
 assert(board.current==128 && saved.brightness==settings::Brightness::High);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as td:
    tmp=Path(td)
    (tmp/"board").mkdir()
    (tmp/"board/board.hpp").write_text(board,encoding="utf-8")
    (tmp/"esp_timer.h").write_text(
        '#pragma once\n#include <cstdint>\nextern std::int64_t fake_us;\n'
        'inline std::int64_t esp_timer_get_time(){return fake_us;}\n')
    (tmp/"test.cpp").write_text(program,encoding="utf-8")
    binary=tmp/"test"
    subprocess.run([
        compiler,"-std=c++17","-Wall","-Wextra","-Werror",
        "-I"+str(tmp),"-I"+str(root/"components/power/include"),
        "-I"+str(root/"components/settings/include"),
        "-I"+str(root/"components/ui_theme/include"),
        str(tmp/"test.cpp"),str(root/"components/power/display_lifecycle.cpp"),
        str(root/"components/settings/settings.cpp"),"-o",str(binary)
    ],check=True)
    subprocess.run([str(binary)],check=True)
print("DisplayLifecycle Active/Dimmed/DisplayOff/POWER/Reserve production test PASS")
