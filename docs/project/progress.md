# Project Progress

## Current milestone

RadioLab v0.1 foundation.

## Current state

- Repository initialized.
- Architecture and working model defined.
- Minimal native ESP-IDF scaffold implemented for the ESP32 target.
- ESP-IDF 5.5.5, M5Unified 0.2.22, and M5GFX 0.2.29 are pinned.
- Hardware sanity application implemented for LCD, Button A/B events, battery/PMU information, device MAC, firmware version, and a startup buzzer command.
- Physical M5StickC Plus SE verification has not yet been completed.

## Next planned implementation step

- Build and flash the hardware sanity firmware on the physical M5StickC Plus SE.
- Verify LCD output and M5Unified board identification.
- Verify the actual physical Button A/B mapping.
- Verify battery/PMU readings, device MAC, firmware version, and audible buzzer output.
- Proceed to minimal radio/protocol foundations only after the hardware sanity checks pass.

## Known blockers

No architectural blockers. Physical hardware verification remains pending.

## Chronological progress

Append concise entries here when the authoritative project state changes.

- 2026-09-18 — Established the initial project documentation foundation and recorded the current architecture, decisions, progress, and workflow.
- 2026-09-18 — Added the minimal native ESP-IDF scaffold and M5StickC Plus SE hardware sanity application.
