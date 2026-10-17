#!/usr/bin/env python3
"""Charging presentation source-level regression assertions.
Full PMU contract remains covered by existing charging tests.
"""
from pathlib import Path
import re
root=Path(__file__).resolve().parents[2]
s=(root/"main/app_main.cpp").read_text(encoding="utf-8")
def body(start,end):
    return s.split(start,1)[1].split(end,1)[0]
render=body("void render_charging_presentation(","void render_charging_full(")
show=body("void show_charging_presentation(","bool update_charging_full_confirmation(")
update=body("void update_charging_presentation(","bool update_charging_full_confirmation(")
sample=body("const nikos::battery_guard::UpdateResult battery_update =","const nikos::board::InputState raw_input =")
assert 'std::snprintf(level, sizeof(level), "%d%%", battery_percent)' in render
assert 'battery_percent >= 0 && battery_percent < 100' in render
assert '(animation_step % 4U) + 1U' in render
assert '"100%"' in render
assert 'render_charging_100(board)' in show
assert 'render_charging_full(board)' in show
assert 'if (state.full_latched)' in update
assert 'if (state.battery_percent == 100)' in update
assert 'state.rendered_percent != state.battery_percent' in update
assert 'state.rendered_percent = state.battery_percent' in update
assert 'state.presentation_started_ms = now;' not in update
assert 'now - state.presentation_started_ms >= kChargingPresentationMs' in update
assert 'display_lifecycle.display_off_now();' in update
assert 'battery_update.sampled' in sample
assert 'battery_update.power_status.voltage_mv > 0' in sample
assert 'battery_update.power_status.level_percent >= 0' in sample
assert 'battery_update.power_status.level_percent <= 100' in sample
assert 'charging_mode.battery_percent =' in sample
# Both unplug and fresh cable connection discard the previous session's 100%.
# Valid readings can still be retained between samples within one session.
reset = body("void reset_charging_session(", "ChargingCableEvent poll_charging_cable(")
connected = body("if (vbus_present && !state.active) {",
                 "if (!vbus_present && state.active) {")
for transition in (reset, connected):
    assert "state.battery_percent = -1;" in transition
    assert "state.rendered_percent = -2;" in transition
full=body("bool update_charging_full_confirmation(","void show_charging_lock(")
assert 'kChargingFullConfirmSamples' in full
assert 'status.charge_state == nikos::board::ChargeState::Discharging' in full
assert 'status.vbus_voltage_mv >= kChargingVbusPresentMv' in full
lock=body("void show_charging_lock(","void render_clock_glance(")
assert 'if (state.full_presentation_pending)' in lock
assert 'board.tone(kChargingFullToneHz, kChargingFullToneMs);' in lock
assert 'state.full_presentation_pending = false;' in lock
assert 'show_charging_presentation(' in lock
assert 'charging_mode.owner_override_active' in s
assert 'charging_wake_requested(raw_input)' in s
assert 'ChargingCableEvent::Disconnected' in s
assert 'communicator.process_incoming()' in s
print("Charging presentation v0.2 source contracts PASS")
