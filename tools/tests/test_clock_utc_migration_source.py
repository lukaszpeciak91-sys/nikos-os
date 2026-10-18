#!/usr/bin/env python3
"""Source contracts for UTC RTC migration, ENT compatibility and scope."""
from pathlib import Path
r=Path(__file__).resolve().parents[2]
read=lambda p:(r/p).read_text(encoding="utf-8")
clock=read("components/clock/clock_service.cpp")
header=read("components/clock/include/clock/clock_service.hpp")
basis=read("components/storage/rtc_time_basis_store.cpp")
nvs=read("components/storage/entertainment_cycle_store.cpp")
main=read("main/app_main.cpp")
launcher=read("components/launcher/launcher.cpp")
guard=read("components/entertainment_guard/include/entertainment_guard/guard.hpp")
assert 'kKey[] = "rtc_basis"' in basis
assert "RtcTimeBasis::LegacyLocal" in basis
assert "RtcTimeBasis::Pending" in basis
assert "RtcTimeBasis::Utc" in basis
assert "RtcTimeBasis::Uncertain" in basis
commit=read("components/clock/include/clock/rtc_utc_commit.hpp")
steps=[
    "ops.save_pending()",
    "ops.mark_pending()",
    "ops.invalidate_date_trust()",
    "ops.write_date(utc)",
    "ops.write_time(utc)",
    "ops.readback(verified)",
    "ops.save_utc_basis()",
    "ops.restore_date_trust()",
    "ops.mark_trusted_utc()",
]
assert all(commit.index(x)<commit.index(y) for x,y in zip(steps,steps[1:]))
for operation in steps:
    assert operation in commit
assert "return commit_utc_checked(utc,operations);" in clock
assert "clock.basis_store_.save(storage::RtcTimeBasis::Pending)" in clock
assert "clock.date_store_.save_configured(false)" in clock
assert "clock.basis_store_.save(storage::RtcTimeBasis::Utc)" in clock
assert "clock.date_store_.save_configured(true)" in clock
assert "if (basis_!=storage::RtcTimeBasis::LegacyLocal || !configured) return;" in clock
assert "warsaw::local_to_utc(local)" in clock
assert "LocalStatus::Unique" in clock
assert "bool ClockService::read_utc(" in clock and "trusted_utc(out)" in clock
assert "warsaw::utc_to_local(raw,converted)" in clock
assert "staged_time_" in clock and "staged_date_" in clock
assert "pending_local_date()" in header
assert "kLegacyMarker=0x454E5431U" in nvs
assert "kUtcMarker=0x454E5432U" in nvs
assert "record.marker==kLegacyMarker" in nvs
assert "const Record record{kUtcMarker,cycle.civil_start" in nvs
assert "if (!saved.active) return;" in guard
assert "fallback_?kBreakMs:kCycleMs" in guard
assert "kWindowMs=20U*60U*1000U" in guard
assert "kBreakMs=60U*60U*1000U" in guard
assert "migrate_legacy_cycle_minutes(stored_entertainment)" in main
assert "legacy_local_minutes_to_utc(" in main
assert "clock.read_utc(utc)" in main
assert "Screen::ClockNeedsDate" in launcher
assert "LocalStatus::Nonexistent" in launcher
assert "LocalStatus::Ambiguous" in launcher
assert "font_core::Mode::PreRendered" in launcher
assert "esp_timer_get_time()" in main
print("RTC UTC / ENT1 ENT2 source checks PASS")
