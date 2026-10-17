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
assert "const bool guard_entry =" in main
entry_gate = main.split("const bool guard_entry =", 1)[1].split(
    "bool entry_refused", 1)[0]
assert all(name in entry_gate for name in ("OpenSnake", "OpenMiniRacer", "OpenFacts"))
assert "OpenSpaceDefender" not in entry_gate
assert "snake.end();" in main and "mini_racer.end();" in main
assert "facts.end();" in main
assert "entertainment_warning_visible" in main
assert "render_entertainment_notice" in main
assert "battery_guard.low_battery_reserve_active()" in main
assert "entertainment_guard.enter(loop_now_ms" in main
assert "is_guarded_entertainment(state)" in main
assert "entertainment_guard.warning_due(loop_now_ms)" in main
assert "entertainment_guard.pending_clear()" in main
assert "entertainment_store.save(" in main
assert "entertainment_store.clear()" in main
store = (root / "components/storage/entertainment_cycle_store.cpp").read_text(encoding="utf-8")
assert "static_assert(sizeof(Record)==12U" in store
assert 'kKey[]="ent_cycle"' in store
assert "warning_shown" in store
assert "resume_snake_after_communicator = false;" in main
assert "resume_mini_racer_after_communicator = false;" in main
assert "resume_facts_after_communicator = false;" in main
assert "kAutoPowerOffIdleMs = 20U * 60U * 1000U;" in main
for game in ("snake", "mini_racer"):
    code = (root / "apps" / game / (game + "_app.cpp")).read_text(encoding="utf-8")
    assert "KONIEC SESJI" in code
    assert "10 MIN GRY" in code
print("Entertainment Guard policy and integration host tests PASS")
