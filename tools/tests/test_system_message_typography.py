#!/usr/bin/env python3
"""System overlay typography regression: no policy or lifecycle changes."""
from pathlib import Path
root=Path(__file__).resolve().parents[2]
main=(root/"main/app_main.cpp").read_text(encoding="utf-8")
font=(root/"components/font_core/include/font_core/font_core.hpp").read_text(encoding="utf-8")
def block(begin,end):
    return main.split(begin,1)[1].split(end,1)[0]
helper=block("void notice_text(","void render_entertainment_notice(")
assert "font_core::Mode::PreRendered" in helper
assert "font_core::Size::Readable" in main
assert "font_core::Size::Compact" in helper
assert "font.measure(text, width, height).fits" in helper
assert "font.draw(text" in helper
assert "void draw(" not in helper
assert "Result measure(" in font
specs=[
 ("void render_entertainment_notice(", "void render_entertainment_warning(",
  ["CZAS NA PRZERWĘ","ROZRYWKA WRÓCI","PÓŹNIEJ",
   "ROZRYWKA","WSTRZYMANA","NAŁADUJ STICK"]),
 ("void render_entertainment_warning(", "enum class ChargingCableEvent",
  ["JESZCZE 2 MINUTY","POTEM PRZERWA"]),
 ("void render_timer_alert(", "void render_battery_advisory(",
  ["MINUTNIK","KONIEC"]),
 ("void render_battery_advisory(", "void render_critical_battery_shutdown(",
  ["BARDZO NISKA","NISKA BATERIA","PODŁĄCZ ŁADOWARKĘ",
   "LUB WYŁĄCZ URZĄDZENIE"]),
 ("void render_critical_battery_shutdown(", "[[noreturn]] void controlled_shutdown(",
  ["NISKA BATERIA","WYŁĄCZAM..."]),
 ("void render_charging_presentation(", "void render_charging_100(",
  ["ŁADOWANIE"]),
]
for start,end,strings in specs:
    content=block(start,end)
    assert "notice_text(" in content, start
    assert "draw_text_region(" not in content or "render_charging_presentation" in start
    for value in strings: assert '"'+value+'"' in content, value
    assert "state_.update(" not in content
assert "render_charging_100(" in main and "render_charging_full(" in main
assert "render_clock_glance(" in main
assert "communicator.process_incoming()" in main
assert "countdown.acknowledge_expiration()" in main
assert "entertainment_guard.enter(" in main
assert "kChargingPresentationMs = 5000" in main
assert "kEntertainmentWarningMs = 2500" in main
assert "kChargingOwnerOverrideIdleMs = 120000" in main
assert "kCriticalShutdownMessageMs = 1750" in main
assert "board.draw_text_region(" in block("void render_clock_glance(","void render_timer_alert(")
print("System message typography source contracts PASS")
