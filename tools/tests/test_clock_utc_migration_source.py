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
commit=clock.split("bool ClockService::commit_utc(",1)[1].split("bool ClockService::write_local(",1)[0]
assert commit.index("basis_store_.save(storage::RtcTimeBasis::Pending)") < commit.index("date_store_.save_configured(false)")
assert commit.index("date_store_.save_configured(false)") < commit.index("board_.write_rtc_date(date)")
assert commit.index("board_.write_rtc_date(date)") < commit.index("board_.write_rtc_time(time)")
assert commit.index("board_.write_rtc_time(time)") < commit.index("raw_rtc(verified)")
assert commit.index("raw_rtc(verified)") < commit.index("basis_store_.save(storage::RtcTimeBasis::Utc)")
assert commit.index("basis_store_.save(storage::RtcTimeBasis::Utc)") < commit.index("date_store_.save_configured(true)")
assert "if (!basis_store_.save(storage::RtcTimeBasis::Pending)) return false;" in commit
assert "if (!basis_store_.save(storage::RtcTimeBasis::Utc)) return false;" in commit
assert "if (!date_store_.save_configured(true)) return false;" in commit
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
