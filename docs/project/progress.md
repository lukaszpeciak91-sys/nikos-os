# Project Progress

## Current milestone

Physically testable RadioLab v0.1 foundation.

## Current state

- Repository architecture and working model defined.
- Native ESP-IDF scaffold implemented for classic ESP32 / ESP32-PICO-D4.
- ESP-IDF 5.5.5, M5Unified 0.2.22, and M5GFX 0.2.29 are pinned.
- Minimal `board`, `radio`, and `protocol` boundaries implemented.
- RadioLab uses the same firmware on both equal peers.
- Versioned DISCOVERY bootstrap, one active peer, PING, application ACK, HELLO, RSSI capture, RTT, recent reachability, NORMAL/LR selection, LIVE periodic PING, and basic counters are implemented.
- ESP-NOW callback-owned RX data and metadata are copied into a FreeRTOS queue before callback return.
- Hardware and radio behavior have not yet been physically verified on the two M5StickC Plus SE units.
- A local ESP-IDF build has not yet been executed in the available implementation environment.

## Next planned implementation step

- Build with ESP-IDF 5.5.5.
- Flash the same firmware to both M5StickC Plus SE devices.
- Verify LCD and the actual physical Button A/B mapping.
- Verify buzzer and battery/AXP192 information.
- Verify automatic peer discovery on channel 6.
- Verify PING -> application ACK, local ACK RSSI, peer-reported PING RSSI, and RTT.
- Verify HELLO retention.
- Verify LIVE link loss/recovery while walking.
- Verify NORMAL and Espressif LR separately on physical hardware.

## Known blockers

No architectural blocker is known. Compile-time validation and physical device verification remain pending.

## Chronological progress

Append concise entries here when the authoritative project state changes.

- 2026-09-18 — Established the initial project documentation foundation and recorded the current architecture, decisions, progress, and workflow.
- 2026-09-18 — Added the minimal native ESP-IDF scaffold and M5StickC Plus SE hardware sanity application.
- 2026-09-18 — Implemented the first equal-peer RadioLab foundation for two-device physical ESP-NOW testing.
