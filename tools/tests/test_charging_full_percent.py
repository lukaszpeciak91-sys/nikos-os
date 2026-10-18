#!/usr/bin/env python3
"""Charging FULL contract regression: source guard plus deterministic scenarios."""
from pathlib import Path

src = (Path(__file__).resolve().parents[2] / "main/app_main.cpp").read_text(encoding="utf-8")
start = src.index("bool update_charging_full_confirmation(")
end = src.index("void show_charging_lock(", start)
logic = src[start:end]
for term in ("status.level_percent == 100",
             "status.charge_state == nikos::board::ChargeState::Discharging",
             "status.vbus_voltage_mv >= kChargingVbusPresentMv",
             "status.voltage_mv > 0",
             "state.full_confirm_count = 0",
             "state.full_latched = true"):
    assert term in logic, term
for forbidden in ("charging_observed", "current_ma", "current_supported",
                  "kChargingFullFallbackBatteryMv", "kChargingFullBatteryMv"):
    assert forbidden not in src, forbidden
assert "kChargingFullConfirmSamples = 3" in src
# Confirmation is produced with the local state argument; the event loop
# sets the same ChargingModeState flag via its charging_mode instance.
assert "charging_mode.full_presentation_pending = true;" in src
assert "if (update_charging_full_confirmation(" in src
assert "if (state.full_presentation_pending)" in src
assert "state.full_presentation_pending = false;" in src
assert "reset_charging_session(ChargingModeState& state)" in src

class Session:
    def __init__(self):
        self.count = 0
        self.latched = False
    def sample(self, *, active=True, valid=True, vbus=True, pct=100,
               charge="Discharging", sampled=True):
        if not active or self.latched or not sampled:
            return False
        if not (valid and vbus and pct == 100 and charge == "Discharging"):
            self.count = 0
            return False
        self.count += 1
        if self.count < 3:
            return False
        self.latched, self.count = True, 0
        return True
    def unplug(self):
        self.count, self.latched = 0, False

s = Session()
assert not s.sample()
assert not s.sample()
assert s.sample() and s.latched
assert not Session().sample(charge="Charging")
assert not Session().sample(pct=99)
for bad in ({"charge":"Unknown"}, {"valid":False}, {"vbus":False}, {"pct":99}):
    s = Session()
    s.sample()
    s.sample()
    assert not s.sample(**bad) and s.count == 0
    assert not s.sample() and s.count == 1
s = Session()
s.sample(); s.sample(); s.unplug()
assert not s.latched and s.count == 0
assert not s.sample() and not s.sample() and s.sample()
# Owner Override does not participate in the confirmation predicate.
assert "owner_override_active" not in logic
print("Charging FULL source and deterministic scenario checks PASS")
