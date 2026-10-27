#!/usr/bin/env python3
"""R1: execute real Launcher handlers and composition-owned NVS transactions."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root=Path(__file__).resolve().parents[2]
launcher_cpp=(root/"components/launcher/launcher.cpp").read_text(encoding="utf-8")
app=(root/"main/app_main.cpp").read_text(encoding="utf-8")
header=(root/"components/launcher/include/launcher/launcher.hpp").read_text(encoding="utf-8")
compiler=shutil.which("g++") or shutil.which("clang++")
assert compiler
assert "storage::UiPreferencesStore" not in header
assert "ui_preferences_store_" not in launcher_cpp
assert '"NIE ZAPISANO"' in launcher_cpp

def balanced(source, needle):
    start=source.index(needle)
    opening=source.index("{",start)
    depth=0
    for i in range(opening,len(source)):
        if source[i]=="{": depth+=1
        elif source[i]=="}":
            depth-=1
            if depth==0:return source[start:i+1]
    raise AssertionError(needle)

update=launcher_cpp.split("Action Launcher::update(",1)[1]
screens=("Settings","SoundMenu","SignalSound","Brightness","Theme","Orientation")
handlers="\n".join(balanced(update,"if (screen_ == Screen::"+name+") {")
                   for name in screens)
helpers=launcher_cpp[launcher_cpp.index("std::uint8_t signal_sound_index("):
                     launcher_cpp.index("\nvoid format_mmss(")]
begin=launcher_cpp.index("std::uint8_t theme_index(")
end=launcher_cpp.index("\n}", launcher_cpp.index("nikos::ui_theme::Theme theme_from_index(",begin))+2
helpers+="\n"+launcher_cpp[begin:end]
methods=[]
for result,name in (
    ("settings::Brightness","requested_brightness"),
    ("ui_theme::Theme","requested_theme"),
    ("settings::Orientation","requested_orientation"),
    ("settings::SignalSound","requested_signal_sound"),
    ("void","complete_preference_change"),
):
    body=balanced(launcher_cpp,result+" Launcher::"+name+"(")
    methods.append(body.replace("Launcher::","Harness::",1))
methods="\n\n".join(methods)

# Extract production action dispatch itself, not a reimplementation of ordering.
begin=app.index("} else if (action == nikos::launcher::Action::SignalSoundChanged)")
end=app.index("} else if (\n                    action == nikos::launcher::Action::AutoPowerOffChanged)",begin)
dispatch=app[begin:end].replace("} else if (action ==", "if (action ==",1)+"}"
for action in ("SignalSoundChanged","ThemeChanged","OrientationChanged","BrightnessChanged"):
    assert "Action::"+action in dispatch
for save in ("save_signal_sound","save_theme","save_orientation","save_brightness"):
    assert "ui_preferences_store."+save in dispatch

harness=r"""
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
#include "settings/settings.hpp"
namespace nikos::board {
enum class DisplayOrientation {Right,Left};
struct InputState {bool primary_short=false,secondary_short=false,secondary_long=false;};
struct Board {
 ui_theme::Theme theme=ui_theme::Theme::Nikos;
 DisplayOrientation orientation=DisplayOrientation::Right;
 int theme_calls=0,orientation_calls=0;
 void set_theme(ui_theme::Theme value){theme=value;++theme_calls;}
 void set_display_orientation(DisplayOrientation value){orientation=value;++orientation_calls;}
};
}
namespace nikos::signal_sound {
struct Player {int plays=0,stops=0;void play_selected(){++plays;}void stop(){++stops;}};
}
namespace nikos::launcher {
enum class Action {None,SignalSoundChanged,ThemeChanged,OrientationChanged,BrightnessChanged};
enum class Screen {Main,Settings,SoundMenu,SignalSound,SignalVolume,Brightness,Theme,Orientation,AutoPowerOff,About};
class Harness {
public:
 settings::State& settings_;
 board::Board& board_;
 signal_sound::Player& signal_sound_;
 Screen screen_=Screen::Main;
 std::uint8_t settings_selection_=0,sound_menu_selection_=0,signal_sound_selection_=0,
     signal_volume_selection_=0,brightness_selection_=0,theme_selection_=0,
     orientation_selection_=0,auto_power_off_selection_=0,about_page_=0;
 bool settings_save_failed_=false;
 int render_count=0;
 std::string footer{};
 void render(){++render_count;footer=settings_save_failed_?"NIE ZAPISANO":"OK";}
 Action update(const board::InputState&);
 settings::Brightness requested_brightness() const;
 ui_theme::Theme requested_theme() const;
 settings::Orientation requested_orientation() const;
 settings::SignalSound requested_signal_sound() const;
 void complete_preference_change(Action action, bool saved);
};
}
struct Store {
 int writes=0;
 bool fail=false;
 std::vector<std::string> events{};
 bool save_signal_sound(nikos::settings::SignalSound){events.push_back("nvs_melody");++writes;return !fail;}
 bool save_theme(nikos::ui_theme::Theme){events.push_back("nvs_theme");++writes;return !fail;}
 bool save_orientation(nikos::settings::Orientation){events.push_back("nvs_orient");++writes;return !fail;}
 bool save_brightness(nikos::settings::Brightness){events.push_back("nvs_bright");++writes;return !fail;}
};
struct FakeLifecycle{
 int applies=0;
 bool asleep=false;
 std::uint8_t active=96,dimmed=24,physical=96;
 void apply(std::uint8_t a,std::uint8_t d){
   ++applies;active=a;dimmed=d;if(!asleep)physical=a;
 }
};
struct FakeBattery {
 bool reserve=false;
 bool low_battery_reserve_active()const{return reserve;}
};
void apply_effective_brightness_profile(
    FakeLifecycle& display_lifecycle,const nikos::settings::State& settings,
    bool reserve)
{
 auto p=nikos::settings::effective_brightness_profile(settings.brightness,reserve);
 display_lifecycle.apply(p.active,p.dimmed);
}
"""
test=r"""
using namespace nikos;
int main(){
 settings::State settings;
 board::Board board;
 signal_sound::Player signal_sound;
 launcher::Harness launcher{settings,board,signal_sound};
 Store ui_preferences_store;
 FakeLifecycle display_lifecycle;
 FakeBattery battery_guard;
 board::InputState m5{};m5.primary_short=true;
 board::InputState side{};side.secondary_short=true;
 board::InputState back{};back.secondary_long=true;
 auto process=[&](launcher::Action action){
 // START_PRODUCTION_DISPATCH
"""
test_end=r"""
 // END_PRODUCTION_DISPATCH
 };
 // MENU entry, direct theme commit and no-op without a second write.
 launcher.screen_=launcher::Screen::Settings;
 launcher.settings_selection_=2;
 launcher.update(m5);
 assert(launcher.screen_==launcher::Screen::Theme
        && launcher.theme_selection_==0 && !launcher.settings_save_failed_);
 launcher.theme_selection_=4;
 auto action=launcher.update(m5);
 assert(action==launcher::Action::ThemeChanged);
 assert(settings.theme==ui_theme::Theme::Nikos);
 assert(board.theme==ui_theme::Theme::Nikos && ui_preferences_store.writes==0);
 process(action);
 assert(settings.theme==ui_theme::Theme::Noir && board.theme==ui_theme::Theme::Noir);
 assert(ui_preferences_store.writes==1 && launcher.footer=="OK");
 process(launcher.update(m5));
 assert(ui_preferences_store.writes==1);
 launcher.update(side); // Normal cursor navigation does not write or keep an old error.
 assert(!launcher.settings_save_failed_ && ui_preferences_store.writes==1);
 // Failed theme keeps the committed runtime/Board and resets the focus.
 ui_preferences_store.fail=true;
 launcher.theme_selection_=2;
 process(launcher.update(m5));
 assert(launcher.settings_save_failed_ && launcher.footer=="NIE ZAPISANO");
 assert(launcher.theme_selection_==4 && settings.theme==ui_theme::Theme::Noir
        && board.theme==ui_theme::Theme::Noir);
 // Exit and re-enter must clear stale error before another confirmation.
 launcher.update(back);
 assert(launcher.screen_==launcher::Screen::Settings);
 launcher.update(m5);
 assert(launcher.screen_==launcher::Screen::Theme && !launcher.settings_save_failed_);
 assert(launcher.footer=="OK" && launcher.theme_selection_==4);
 // Retry: save succeeds without requiring an extra confirmation button.
 ui_preferences_store.fail=false;
 launcher.theme_selection_=2;
 process(launcher.update(m5));
 assert(settings.theme==ui_theme::Theme::Matrix && board.theme==ui_theme::Theme::Matrix);
 assert(!launcher.settings_save_failed_);
 // Orientation: no rotation on NVS failure; success applies only after saving.
 launcher.screen_=launcher::Screen::Settings;
 launcher.settings_selection_=3;
 launcher.update(m5);
 assert(!launcher.settings_save_failed_);
 launcher.orientation_selection_=1;
 ui_preferences_store.fail=true;
 process(launcher.update(m5));
 assert(launcher.orientation_selection_==0 && settings.orientation==settings::Orientation::Right);
 assert(board.orientation==board::DisplayOrientation::Right);
 ui_preferences_store.fail=false;
 launcher.orientation_selection_=1;
 process(launcher.update(m5));
 assert(settings.orientation==settings::Orientation::Left
        && board.orientation==board::DisplayOrientation::Left);
 int calls=ui_preferences_store.writes;
 process(launcher.update(m5));
 assert(ui_preferences_store.writes==calls);
 // Brightness: composition owns Reserve cap and DisplayOff.
 launcher.screen_=launcher::Screen::Settings;launcher.settings_selection_=1;
 launcher.update(m5);
 launcher.brightness_selection_=2;
 battery_guard.reserve=true;
 process(launcher.update(m5));
 assert(settings.brightness==settings::Brightness::High);
 assert(display_lifecycle.active==72 && display_lifecycle.dimmed==18);
 display_lifecycle.asleep=true;display_lifecycle.physical=0;
 ui_preferences_store.fail=true;launcher.brightness_selection_=0;
 process(launcher.update(m5));
 assert(settings.brightness==settings::Brightness::High);
 assert(launcher.brightness_selection_==2 && display_lifecycle.physical==0);
 ui_preferences_store.fail=false;battery_guard.reserve=false;
 launcher.brightness_selection_=0;
 process(launcher.update(m5));
 assert(settings.brightness==settings::Brightness::Low
        && display_lifecycle.physical==0);
 calls=ui_preferences_store.writes;
 process(launcher.update(m5));
 assert(ui_preferences_store.writes==calls);
 // Melody: composition applies preview after committed save, even on no-op.
 launcher.screen_=launcher::Screen::Settings;launcher.settings_selection_=0;
 launcher.update(m5);
 assert(launcher.screen_==launcher::Screen::SoundMenu);
 launcher.update(m5);
 assert(launcher.screen_==launcher::Screen::SignalSound && !launcher.settings_save_failed_);
 launcher.signal_sound_selection_=2;
 int previews=signal_sound.plays;
 action=launcher.update(m5);
 assert(action==launcher::Action::SignalSoundChanged);
 assert(signal_sound.plays==previews);
 process(action);
 assert(settings.signal_sound==settings::SignalSound::Pager && signal_sound.plays==previews+1);
 calls=ui_preferences_store.writes;
 process(launcher.update(m5));
 assert(ui_preferences_store.writes==calls && signal_sound.plays==previews+2);
 ui_preferences_store.fail=true;launcher.signal_sound_selection_=1;
 process(launcher.update(m5));
 assert(settings.signal_sound==settings::SignalSound::Pager
        && launcher.signal_sound_selection_==2);
 assert(signal_sound.plays==previews+2 && launcher.footer=="NIE ZAPISANO");
 launcher.update(back);
 assert(launcher.screen_==launcher::Screen::SoundMenu);
 launcher.update(m5);
 assert(launcher.screen_==launcher::Screen::SignalSound
        && launcher.signal_sound_selection_==2 && !launcher.settings_save_failed_);
 ui_preferences_store.fail=false;launcher.signal_sound_selection_=0;
 process(launcher.update(m5));
 assert(settings.signal_sound==settings::SignalSound::Gentle
        && launcher.footer=="OK");
 return 0;
}
"""
code=harness+"\n"+helpers+"\nnamespace nikos::launcher {\nlauncher::Action Harness::update(const board::InputState& input){\n"+handlers+"\nreturn Action::None;\n}\n"+methods+"\n}\n"+test+dispatch+test_end
with tempfile.TemporaryDirectory() as folder:
    tmp=Path(folder)
    src=tmp/"production.cpp"
    binary=tmp/"production"
    src.write_text(code,encoding="utf-8")
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror",
                    "-I"+str(root/"components/settings/include"),
                    "-I"+str(root/"components/ui_theme/include"),
                    str(src),str(root/"components/settings/settings.cpp"),
                    "-o",str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
print("Settings R1 production Launcher and composition confirmation/failure/reentry PASS")
