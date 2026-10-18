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
int main() {
    constexpr std::uint32_t minute = 60000;
    Guard g;
    assert(g.phase(0)==Phase::Available);
    // Opening the Entertainment menu never calls enter().
    assert(g.phase(700*minute)==Phase::Available);
    assert(g.enter(700*minute,false,0));   // Snake
    assert(g.pending_write());
    assert(g.snapshot().active && !g.snapshot().civil_valid);
    g.settled();
    assert(g.enter(715*minute,false,0));   // Facts / Quiz
    assert(g.enter(717*minute,false,0));   // Mini Racer
    assert(!g.pending_write());
    assert(!g.warning_due(717*minute+59999));
    assert(g.warning_due(718*minute));
    g.acknowledge_warning();
    assert(g.pending_write());
    assert(g.snapshot().warning_shown);
    g.settled();
    assert(!g.warning_due(718*minute));
    assert(!g.warning_due(719*minute));
    assert(g.phase(720*minute)==Phase::Cooldown);
    assert(!g.enter(720*minute,true,200000));
    assert(!g.enter(779*minute,true,200000));
    assert(g.phase(780*minute)==Phase::Available);
    assert(g.pending_clear());
    g.settled();
    assert(g.enter(780*minute,false,0));
    assert(g.pending_write());
    // Trusted civil-minute restoration counts time powered off.
    Guard fresh;
    fresh.restore({true,true,20000},1500000,true,20005);
    assert(fresh.phase(1500000)==Phase::Window);
    assert(fresh.warning_due(1500000+13*minute));
    Guard restored_warning;
    restored_warning.restore({true,true,20000,true},1500000,true,20018);
    assert(!restored_warning.warning_due(1500000));
    Guard locked;
    locked.restore({true,true,20000},1500000,true,20030);
    assert(locked.phase(1500000)==Phase::Cooldown);
    assert(!locked.enter(1500001,true,20030));
    Guard expired;
    expired.restore({true,true,20000},1500000,true,20080);
    assert(expired.phase(1500000)==Phase::Available);
    assert(expired.pending_clear());
    // Without a trustworthy RTC/date, persisted state cannot grant play.
    Guard untrusted;
    untrusted.restore({true,true,20000},1000,false,0);
    assert(untrusted.phase(1000)==Phase::Cooldown);
    assert(!untrusted.enter(1000,true,20001));
    assert(untrusted.phase(1000+59*minute)==Phase::Cooldown);
    assert(untrusted.phase(1000+60*minute)==Phase::Available);
    assert(untrusted.pending_clear());
    Guard marker_only;
    marker_only.restore({true,false,0},500,false,0);
    assert(marker_only.phase(500)==Phase::Cooldown);
    assert(marker_only.phase(500+60*minute)==Phase::Available);
    // Fault injection: storage refuses the first transition write.
    Guard first_write_failure;
    assert(first_write_failure.enter(0,true,20000));
    assert(first_write_failure.pending_write());
    const bool injected_first_save_ok=false;
    if (!injected_first_save_ok) first_write_failure.abort_unpersisted_start();
    assert(first_write_failure.phase(0)==Phase::Available);
    assert(!first_write_failure.pending_write());
    // Recovery after NVS becomes writable must start a new persisted cycle.
    assert(first_write_failure.enter(1000,true,20000));
    assert(first_write_failure.pending_write());
    first_write_failure.settled();
    assert(first_write_failure.phase(1000)==Phase::Window);
    // Failed expiry clear remains pending until an explicit successful clear.
    Guard failed_clear;
    assert(failed_clear.enter(0,true,20000));
    failed_clear.settled();
    assert(failed_clear.phase(80*minute)==Phase::Available);
    assert(failed_clear.pending_clear());
    const bool injected_clear_ok=false;
    if(injected_clear_ok) failed_clear.settled();
    assert(failed_clear.pending_clear());
    failed_clear.settled(); // simulated recovery
    assert(!failed_clear.pending_clear());
    // Repeated phase reads do not request periodic writes.
    Guard monotonic;
    assert(monotonic.enter(0,false,0));
    monotonic.settled();
    for (std::uint32_t now=1;now<20*minute;now+=17)
        (void)monotonic.phase(now);
    assert(!monotonic.pending_write() && !monotonic.pending_clear());
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
    "bool trusted_civil_minute",1)[0]
def check_contract(screen_text, update_text, recognized_text, runtime_text):
    # The screen origin is tagged once before dispatch; individual routes
    # must not rely on manual wrapping or enum numeric position.
    if "entertainment_launch_ = screen_ == Screen::Entertainment\n        || screen_ == Screen::Games;" not in update_text:
        return False
    if "entertainment_action(" in screen_text:
        return False
    routes = re.findall(r"return\s+Action::(Open\w+)\s*;", screen_text)
    if len(routes)!=5 or len(routes)!=len(set(routes)):
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
    "bool trusted_civil_minute", 1)[0]
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
assert "KONIEC SESJI" in space_code
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
assert "entertainment_clear_failed" in main
assert "if (entertainment_store.clear())" in main
assert "&& !entertainment_warning_visible" in main
restoration = main.split("if ((resume_snake_after_communicator",1)[1].split("communicator_result",1)[0]
assert "resume_space_defender_after_communicator" in restoration
assert "resume_coin_flip_after_communicator" in restoration
assert "entertainment_store.clear()" in main
store = (root / "components/storage/entertainment_cycle_store.cpp").read_text(encoding="utf-8")
assert "static_assert(sizeof(Record)==12U" in store
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
assert "Action::OpenSnake" not in menu
assert "Action::OpenFacts" not in games
assert "(entertainment_selection_ + 1U) % 3U" in menu
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
