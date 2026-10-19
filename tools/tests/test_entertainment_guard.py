#!/usr/bin/env python3
"""Exercise the production Entertainment Guard C++17 policy and integration contracts."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
compiler = shutil.which("g++") or shutil.which("clang++")
if not compiler:
    raise SystemExit("SKIP: C++17 host compiler unavailable")
source = r"""
#include <cassert>
#include <cstdint>
#include "entertainment_guard/guard.hpp"
using namespace nikos::entertainment_guard;
constexpr std::uint32_t m = 60000U;
constexpr std::uint64_t base = 100000U;
void tick(Guard& g, std::uint32_t now, bool visible,
          bool trusted=true, std::uint64_t utc=0) {
    g.tick(now, visible, trusted, utc ? utc : base + now/1000U);
    if (g.pending_write()) g.settled(); // Simulated durable NVS commit.
}
int main() {
    Guard g;
    g.restore({},0,true,base);
    assert(g.phase(0)==Phase::Available);
    tick(g,5*m,false);
    assert(g.phase(5*m)==Phase::Available); // Menu is free.
    assert(g.enter(5*m,true,base+300U));
    assert(g.snapshot().used_ms==Guard::kCheckpointMs); // Ahead reservation.
    g.settled();
    tick(g,5*m,true);
    tick(g,13*m,false); // Played 8m.
    assert(g.used_ms()==8U*m);
    tick(g,18*m,false);
    assert(g.enter(18*m,true,base+18U*60U));
    g.settled();
    tick(g,18*m,true);
    tick(g,28*m,true); // 18m cumulative.
    assert(g.warning_due(28*m));
    g.acknowledge_warning();
    assert(g.snapshot().warning_shown);
    g.settled();
    assert(!g.warning_due(28*m));
    tick(g,30*m,true); // Exactly 20m cumulative, not 20m since first start.
    assert(g.phase(30*m)==Phase::Cooldown);
    assert(!g.enter(30*m,true,base+1800U));
    assert(g.phase(30*m+Guard::kBreakMs-1U)==Phase::Cooldown);
    assert(g.phase(30*m+Guard::kBreakMs)==Phase::Available);
    assert(g.pending_clear());

    Guard reset;
    assert(reset.enter(0,true,base));
    reset.settled();
    tick(reset,0,true);
    tick(reset,8*m,false);
    assert(reset.used_ms()==8*m);
    tick(reset,23*m-1U,false);
    assert(reset.phase(23*m-1U)==Phase::Window);
    tick(reset,23*m,false);
    assert(reset.phase(23*m)==Phase::Available);
    assert(reset.pending_clear());
    reset.settled();
    assert(reset.enter(23*m,true,base+23U*60U));
    assert(reset.used_ms()==0);
    reset.settled();

    // Visible in-app menu counts; screen off / communicator / warning does not.
    Guard visibility;
    assert(visibility.enter(0,true,base));
    visibility.settled();
    tick(visibility,0,true);
    tick(visibility,7*m,true); // Includes in-app menu.
    tick(visibility,7*m,false); // Communicator takeover.
    tick(visibility,10*m,false); // No buttons, no counting.
    assert(visibility.used_ms()==7U*m);
    assert(visibility.enter(10*m,true,base+10U*60U));
    visibility.settled();
    tick(visibility,10*m,true);
    tick(visibility,11*m,false); // DisplayOff.
    assert(visibility.used_ms()==8U*m);

    // ENT3 active checkpoint survives reboot, charging reserved seconds.
    Guard active;
    assert(active.enter(0,true,base));
    const auto initial_reserved=active.snapshot();
    assert(!initial_reserved.paused && initial_reserved.used_ms==m);
    active.settled();
    tick(active,0,true);
    tick(active,8*m,true);
    const auto active_disk=active.snapshot();
    assert(active_disk.used_ms==9*m);
    Guard restarted;
    restarted.restore(active_disk,700,true,base+8U*60U);
    assert(restarted.phase(700)==Phase::Window);
    assert(restarted.used_ms()==9*m); // Never a free reboot reset.
    tick(restarted,700,false,true,base+8U*60U);
    tick(restarted,700+Guard::kIdleResetMs-1U,false,true,base+8U*60U+899U);
    assert(restarted.phase(700+Guard::kIdleResetMs-1U)==Phase::Window);
    tick(restarted,700+Guard::kIdleResetMs,false,true,base+8U*60U+900U);
    assert(restarted.phase(700+Guard::kIdleResetMs)==Phase::Available);

    // Inactivity deadline restores when the saved state was already paused.
    Guard idle;
    assert(idle.enter(0,true,base));
    idle.settled();
    tick(idle,0,true);
    tick(idle,8*m,false);
    const auto paused_disk=idle.snapshot();
    assert(paused_disk.paused && paused_disk.used_ms==8*m);
    Guard idle_reboot;
    idle_reboot.restore(paused_disk,1000,true,base+12U*60U);
    assert(idle_reboot.phase(1000)==Phase::Window);
    tick(idle_reboot,1000+11*m-1U,false,true,base+23U*60U-1U);
    assert(idle_reboot.phase(1000+11*m-1U)==Phase::Window);
    tick(idle_reboot,1000+11*m,false,true,base+23U*60U);
    assert(idle_reboot.phase(1000+11*m)==Phase::Available);

    // Cooldown reboot retains the UTC deadline; rollback/untrusted gets 60m.
    Guard cooling;
    cooling.restore({true,true,0,true,Guard::kWindowMs,
                     base+20U*60U,false,true,false},
                    1500,true,base+40U*60U);
    assert(cooling.phase(1500)==Phase::Cooldown);
    assert(cooling.phase(1500+39*m)==Phase::Cooldown);
    assert(cooling.phase(1500+40*m)==Phase::Available);
    Guard rollback;
    rollback.restore(paused_disk,0,true,base-100U);
    assert(rollback.phase(0)==Phase::Cooldown);
    Guard missing_rtc;
    missing_rtc.restore(paused_disk,0,false,0);
    assert(missing_rtc.phase(0)==Phase::Cooldown);
    assert(missing_rtc.phase(Guard::kBreakMs-1U)==Phase::Cooldown);
    assert(missing_rtc.phase(Guard::kBreakMs)==Phase::Available);
    Guard live_rollback;
    assert(live_rollback.enter(0,true,base));
    live_rollback.settled();
    tick(live_rollback,0,true,true,base+100U);
    tick(live_rollback,m,true,true,base+1U);
    assert(live_rollback.phase(m)==Phase::Cooldown);

    // A failed save denies play. A failed clear stays pending until success.
    Guard nvs_failure;
    assert(nvs_failure.enter(0,true,base));
    assert(nvs_failure.pending_write());
    nvs_failure.abort_unpersisted_start();
    assert(nvs_failure.phase(0)==Phase::Cooldown);
    assert(!nvs_failure.enter(1000,true,base));
    Guard clear_failure;
    assert(clear_failure.enter(0,true,base));
    clear_failure.settled();
    tick(clear_failure,0,true);
    tick(clear_failure,Guard::kWindowMs,true);
    assert(clear_failure.phase(Guard::kWindowMs)==Phase::Cooldown);
    assert(clear_failure.phase(Guard::kWindowMs+Guard::kBreakMs)==Phase::Available);
    assert(clear_failure.pending_clear());
    const bool simulated_commit_ok=false;
    if (simulated_commit_ok) clear_failure.settled();
    assert(!clear_failure.enter(Guard::kWindowMs+Guard::kBreakMs,true,base));
    clear_failure.settled();
    assert(clear_failure.enter(Guard::kWindowMs+Guard::kBreakMs,true,base));

    // 32-bit monotonic wrap: elapsed 20 minutes still charged accurately.
    Guard wrap;
    const std::uint32_t at=0xFFF00000U;
    assert(wrap.enter(at,true,base));
    wrap.settled();
    tick(wrap,at,true,true,base);
    tick(wrap,at+Guard::kWindowMs,true,true,base+1200U);
    assert(wrap.phase(at+Guard::kWindowMs)==Phase::Cooldown);
    return 0;
}
"""
with tempfile.TemporaryDirectory() as tmp:
    target = Path(tmp) / "ent_guard_host.cpp"
    binary = Path(tmp) / "ent_guard_host"
    target.write_text(source, encoding="utf-8")
    subprocess.run([
        compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
        "-I", str(root / "components/entertainment_guard/include"),
        str(target), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

main = (root / "main/app_main.cpp").read_text(encoding="utf-8")
assert "OpenSnake" in main and "OpenMiniRacer" in main and "OpenFacts" in main
assert "OpenCoinFlip" in main
assert "const bool guard_entry = launcher.take_entertainment_launch()" in main
assert "&& action != nikos::launcher::Action::None;" in main
assert "guard_entry != recognized_entertainment" in main
assert "if (guard_entry && !entry_refused)" in main
launcher = (root / "components/launcher/launcher.cpp").read_text(encoding="utf-8")
header = (root / "components/launcher/include/launcher/launcher.hpp").read_text(encoding="utf-8")
import re
screen = launcher.split("if (screen_ == Screen::Entertainment)",1)[1].split(
    "if (screen_ == Screen::Clock)",1)[0]
update_start = launcher.split("Action Launcher::update(",1)[1].split(
    "if (screen_ == Screen::Main)",1)[0]
recognized = main.split("const bool recognized_entertainment =",1)[1].split(
    "bool entry_refused",1)[0]
predicate = main.split("bool is_guarded_entertainment(",1)[1].split(
    "bool trusted_utc_second",1)[0]
def check_contract(screen_text, update_text, recognized_text, runtime_text):
    # The screen origin is tagged once before dispatch; individual routes
    # must not rely on manual wrapping or enum numeric position.
    if "entertainment_launch_ = screen_ == Screen::Entertainment\n        || screen_ == Screen::Games;" not in update_text:
        return False
    if "entertainment_action(" in screen_text:
        return False
    routes = re.findall(r"return\s+Action::(Open\w+)\s*;", screen_text)
    if len(routes)!=6 or len(routes)!=len(set(routes)):
        return False
    for action in routes:
        if "Action::"+action not in recognized_text:
            return False
        if "RuntimeState::"+action.removeprefix("Open") not in runtime_text:
            return False
    return True
assert check_contract(screen,update_start,recognized,predicate)
# A synthetic unknown route must be rejected by central recognition
# provenance; without action/runtime integration it MUST fail validation.
synthetic = screen.replace("return Action::OpenSnake;",
                           "return Action::OpenSyntheticGame;")
assert not check_contract(synthetic,update_start,recognized,predicate)
assert not check_contract(screen,update_start.replace(
    "entertainment_launch_ = screen_ == Screen::Entertainment\n        || screen_ == Screen::Games;",""),
    recognized,predicate)
assert not check_contract(screen,update_start,recognized,
                          predicate.replace("RuntimeState::SpaceDefender",""))
assert "return Action::None;" in screen  # BACK/navigation does not authorize
assert "bool take_entertainment_launch()" in header

termination = main.split("if (entertainment_locked || reserve_entertainment)",1)[1].split(
    "if (update_charging_full_confirmation(",1)[0]
resume = main.split("if ((resume_snake_after_communicator",1)[1].split(
    "if (timer_deferred_for_communication",1)[0]
warning = main.split("if (entertainment_warning_visible &&",1)[1].split(
    "if (state == RuntimeState::Launcher)",1)[0]
def lifecycle_covered(termination_text, resume_text, warning_text):
    for name, variable in (
        ("Snake","snake"), ("MiniRacer","mini_racer"),
        ("SpaceDefender","space_defender"), ("CoinFlip","coin_flip"),
        ("Facts","facts")):
        flag = "resume_" + variable + "_after_communicator"
        if variable+".end();" not in termination_text:
            return False
        if flag+" = false;" not in termination_text:
            return False
        if flag not in resume_text:
            return False
    for operation in ("mini_racer.suspend();", "space_defender.suspend();",
                      "mini_racer.resume();", "space_defender.resume();",
                      "coin_flip.suspend();", "coin_flip.resume();",
                      "snake.suspend_for_entertainment_warning();",
                      "snake.resume_from_entertainment_warning()", "facts.redraw()"):
        if operation not in warning_text:
            return False
    return True
assert lifecycle_covered(termination, resume, warning)
assert not lifecycle_covered(termination.replace("space_defender.end();",""),
                             resume, warning)
assert not lifecycle_covered(termination.replace(
    "resume_space_defender_after_communicator = false;",""),resume,warning)
assert not lifecycle_covered(termination,resume.replace(
    "resume_space_defender_after_communicator",""),warning)
assert not lifecycle_covered(termination,resume,warning.replace(
    "space_defender.suspend();",""))
# No rendering of Launcher is allowed in the early global-expiry branch.
assert "launcher.begin_entertainment(" not in termination
assert "entertainment_notice_rendered = false;" in termination
assert "(charging_mode.active && !charging_mode.owner_override_active)" in main
assert "render_entertainment_notice(board, entertainment_notice);" in main
# The existing DisplayLifecycle wake path must not resume during warning.
for name in ("mini_racer", "space_defender", "coin_flip"):
    path = main.split(name+".resume_if_suspended();",1)[0].rsplit("if (",1)[1]
    assert "!entertainment_warning_visible" in path

assert "snake.end();" in main and "mini_racer.end();" in main
assert "facts.end();" in main
assert "space_defender.end();" in main
assert "coin_flip.end();" in main
predicate = main.split("bool is_guarded_entertainment(", 1)[1].split(
    "bool trusted_utc_second", 1)[0]
assert "RuntimeState::SpaceDefender" in predicate
assert "RuntimeState::CoinFlip" in predicate
termination = main.split("if (entertainment_locked || reserve_entertainment)", 1)[1].split(
    "if (update_charging_full_confirmation(", 1)[0]
assert "resume_space_defender_after_communicator" in termination
assert "resume_coin_flip_after_communicator" in termination
assert "space_defender.end();" in termination
assert "coin_flip.end();" in termination
assert "EntertainmentNotice::Break" in termination
assert "app_was_preempted" in termination
assert "resume_space_defender_after_communicator = false;" in termination
assert "entertainment_notice = reserve_entertainment" in termination
warning = main.split("if (entertainment_warning_visible &&", 1)[1].split(
    "if (state == RuntimeState::Launcher)", 1)[0]
assert "space_defender.suspend();" in warning
assert "space_defender.resume();" in warning
assert "space_defender.begin();" in main
assert "coin_flip.begin();" in main
communicator_exit = main.split("communicator_result", 1)[1]
assert "entertainment_notice != EntertainmentNotice::None" in communicator_exit
assert "launcher.begin_entertainment(" in communicator_exit
assert "entertainment_notice_rendered = false;" in communicator_exit
space_code = (root / "apps/space_defender/space_defender_app.cpp").read_text(encoding="utf-8")
assert "KONIEC SESJI" not in space_code
assert "KONIEC CZASU" in space_code
assert "world_.mission_enabled=true;" in space_code
assert "10 MIN GRY" in space_code
assert '"CZAS NA PRZERWĘ"' not in space_code
assert "rules::kSessionMs" in space_code
assert "entertainment_warning_visible" in main
assert "render_entertainment_notice" in main
assert "battery_guard.low_battery_reserve_active()" in main
assert "entertainment_guard.enter(loop_now_ms" in main
assert "is_guarded_entertainment(state)" in main
assert "entertainment_guard.warning_due(loop_now_ms)" in main
assert "entertainment_guard.pending_clear()" in main
assert "entertainment_store.save(" in main
assert "abort_unpersisted_start()" in main
assert "entertainment_guard.tick(" in main
assert "const bool entertainment_visible =" in main
assert "entertainment_guard.storage_failure(" in main
assert "entertainment_clear_failed" in main
assert "if (entertainment_store.clear())" in main
assert "&& !entertainment_warning_visible" in main
restoration = main.split("if ((resume_snake_after_communicator",1)[1].split("communicator_result",1)[0]
assert "resume_space_defender_after_communicator" in restoration
assert "resume_coin_flip_after_communicator" in restoration
assert "entertainment_store.clear()" in main
store = (root / "components/storage/entertainment_cycle_store.cpp").read_text(encoding="utf-8")
assert "static_assert(sizeof(Record) == 24U" in store
assert 'kKey[]="ent_cycle"' in store
assert "warning_shown" in store
assert "resume_snake_after_communicator = false;" in main
assert "resume_mini_racer_after_communicator = false;" in main
assert "resume_coin_flip_after_communicator = false;" in main
assert "resume_facts_after_communicator = false;" in main
assert "kAutoPowerOffIdleMs = 20U * 60U * 1000U;" in main
# Snake now relies exclusively on the central Guard for play-window timing.
snake_code = (root / "apps/snake/snake_app.cpp").read_text(encoding="utf-8")
assert "KONIEC SESJI" not in snake_code
assert "10 MIN GRY" not in snake_code
assert "State::GameOver" in snake_code
# Mini Racer's existing 10-minute session is intentionally unchanged in R0.
racer_code = (root / "apps/mini_racer/mini_racer_app.cpp").read_text(encoding="utf-8")
assert "KONIEC SESJI" not in racer_code
assert "KONIEC CZASU" in racer_code
assert "round_.begin(now);" in racer_code
print("Entertainment Guard policy and integration host tests PASS")

# Every entertainment action including CoinFlip is covered by the one-use
# provenance gate and central guard admission. No direct unguarded entry.
assert "action == nikos::launcher::Action::OpenCoinFlip" in recognized
assert "return Action::OpenCoinFlip;" in screen
assert "coin_flip.begin();" in main
assert "resume_coin_flip_after_communicator" in main
assert "if (entertainment_locked || reserve_entertainment)" in main
assert "entertainment_guard.warning_due(loop_now_ms)" in main
assert "coin_flip.suspend();" in warning
assert "coin_flip.resume();" in warning
assert "coin_flip.end();" in termination
# Repeated tosses are confined to the app; it never starts a new guard window.
app = (root / "apps/coin_flip/coin_flip_app.cpp").read_text(encoding="utf-8")
assert "entertainment_guard" not in app and "nvs_" not in app
assert "esp_random()" in app
assert "if (input.primary_short && !animation_at_entry)" in app

# The hierarchy is explicit; entering ROZRYWKA or GRY returns None.
menu = launcher.split("if (screen_ == Screen::Entertainment)",1)[1].split(
    "if (screen_ == Screen::Games)",1)[0]
games = launcher.split("if (screen_ == Screen::Games)",1)[1].split(
    "if (screen_ == Screen::Clock)",1)[0]
assert "screen_ = Screen::Games;" in menu
assert "screen_ = Screen::Entertainment;" in games
assert "return Action::OpenFacts;" in menu
assert "return Action::OpenVwGarage;" in menu
assert "Action::OpenSnake" not in menu
assert "Action::OpenFacts" not in games
assert "Action::OpenVwGarage" not in games
assert "(entertainment_selection_ + 1U) % 4U" in menu
assert "(games_selection_ + 1U) % 5U" in games
for action in ("OpenSnake", "OpenMiniRacer", "OpenSpaceDefender", "OpenCoinFlip"):
    assert ("return Action::"+action+";") in games
    assert ("Action::"+action) in recognized
assert "return Action::OpenFacts;" in menu
assert "launcher.begin_games(" in main
assert main.count("launcher.begin_games(") == 4
assert "void Launcher::begin_games(" in launcher
# Returning to GRY must render only GRY, never flash ROZRYWKA first.
games_entry = launcher.split("void Launcher::begin_games(",1)[1].split(
    "void Launcher::restore_signal_volume_preference(",1)[0]
assert "begin_entertainment(communicator_status);" not in games_entry
assert games_entry.count("render();") == 1
assert "screen_ = Screen::Games;" in games_entry
assert "reset_stopwatch_session();" in games_entry
assert "update_clock_sample(now, true);" in games_entry
assert "update_battery_sample(now);" in games_entry
assert "entertainment_guard.enter(loop_now_ms" in main
assert "guard_entry != recognized_entertainment" in main
assert "&& action != nikos::launcher::Action::None;" in main
assert "if (guard_entry && !entry_refused)" in main
# Gameplay and warning logic remain centrally guarded; menus never call Guard.
assert "entertainment_guard" not in games
assert "entertainment_guard" not in menu
assert "bool is_guarded_entertainment(" in main
