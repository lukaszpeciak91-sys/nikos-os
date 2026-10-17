#!/usr/bin/env python3
"""Check explicit Launcher category order and nested game exit targets."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
launcher=(root/"components/launcher/launcher.cpp").read_text(encoding="utf-8")
header=(root/"components/launcher/include/launcher/launcher.hpp").read_text(encoding="utf-8")
main=(root/"main/app_main.cpp").read_text(encoding="utf-8")
def section(start,end):
    return launcher.split(start,1)[1].split(end,1)[0]
tools=section("void Launcher::render_tools()","void Launcher::render_entertainment()")
ent=section("void Launcher::render_entertainment()","void Launcher::render_games()")
games=section("void Launcher::render_games()","void Launcher::render_clock()")
for text,expected in (
    (tools,["LATARKA","MIERNIK HAŁASU","PRZELICZNIK","POWERDIAG","POWRÓT"]),
    (ent,["GRY","CIEKAWOSTKI","POWRÓT"]),
    (games,["SNAKE","MINI RACER","SPACE DEFENDER","RZUT MONETĄ","POWRÓT"])):
    indices=[text.index('"'+x+'"') for x in expected]
    assert indices==sorted(indices)
    assert 'slot < (readable ? 3U : 4U)' in text or 'visible' in text
assert '"KOMUNIKATOR"' in launcher and '"ZEGAR"' in launcher
assert "Screen::Games" in header
tools_input=section("if (screen_ == Screen::Tools)","if (screen_ == Screen::Entertainment)")
for i,action in enumerate(("OpenFlashlight","OpenSoundMeter","OpenConverter","OpenPowerDiag")):
    assert f"tools_selection_ == {i}U" in tools_input
    assert f"Action::{action}" in tools_input
assert "tools_selection_ + 1U) % 5U" in tools_input
menu_input=section("if (screen_ == Screen::Entertainment)","if (screen_ == Screen::Games)")
game_input=section("if (screen_ == Screen::Games)","if (screen_ == Screen::Clock)")
assert "screen_ = Screen::Games;" in menu_input
assert "return Action::OpenFacts;" in menu_input
assert "screen_ = Screen::Entertainment;" in game_input
assert "screen_ = Screen::Main;" in menu_input
assert "return Action::None;" in menu_input and "return Action::None;" in game_input
assert "entertainment_launch_ = screen_ == Screen::Entertainment" in launcher
assert "|| screen_ == Screen::Games;" in launcher
assert main.count("launcher.begin_games(")==4
assert "launcher.begin_entertainment(" in main
assert "guard_entry != recognized_entertainment" in main
assert "entertainment_guard.enter(loop_now_ms" in main
assert "ChargingCableEvent::Connected" in main
assert "ChargingCableEvent::Disconnected" in main
assert "display_lifecycle.state()" in main
assert "communicator.process_incoming()" in main
print("Launcher navigation source contracts PASS")
