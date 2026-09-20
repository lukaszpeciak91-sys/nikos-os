# Project Progress

## Current milestone

Communicator v0.1 core UX over the long-lived messaging infrastructure.

## Current state

- Repository architecture and working model defined.
- Native ESP-IDF scaffold implemented for classic ESP32 / ESP32-PICO-D4.
- ESP-IDF 5.5.5, M5Unified 0.2.22, and M5GFX 0.2.29 are pinned.
- Minimal `board`, `radio`, and `protocol` boundaries implemented.
- Boot shows a short deterministic branded signal-synchronization splash and enters a dark navy five-entry launcher: Communicator, RadioLab, Minutnik, Rozrywka, WYŁĄCZ. A local four-row viewport keeps the header, battery indicator, and footer clear.
- Communicator and RadioLab are real launcher applications; Minutnik and Rozrywka remain visible placeholders.
- RadioLab uses the same firmware on both equal peers.
- Versioned DISCOVERY bootstrap, one active peer, PING, application ACK, HELLO, RSSI capture, RTT, recent reachability, and explicit NORMAL/LR selection are implemented.
- Launcher controls use primary short = open/confirm and secondary short = next.
- RadioLab controls use secondary short = PING, primary short = HELLO, primary long = NORMAL/LR, and secondary long = return to launcher.
- The separate power button is not part of launcher or application navigation.
- Launcher `WYŁĄCZ` now provides explicit whole-device shutdown through a default-NIE confirmation; shutdown cleanup is orchestrated by `app_main`, while M5-specific power-off remains inside `board`.
- The board rendering layer includes a small embedded DejaVu Sans subset for ASCII plus Polish UI letters, exposed only through a scoped Polish UTF-8 text API.
- The first Nikoś OS reference theme now uses a near-black navy base, dark-navy selection surfaces, warm ivory text, restrained mint status/selection accents, subdued cool secondary text, and orange reserved for attention actions such as `SYGNAŁ`.
- ESP-NOW callback-owned RX data and metadata are copied into a FreeRTOS queue before callback return.
- A long-lived `messaging::Service` now owns one-peer Communicator presence, reachability, retry/ACK delivery, dedupe, and experimental RX profiles above `radio`.
- Communicator background messaging starts OFF after boot and is explicitly enabled/disabled from the launcher for the current OS session only; the state is not persisted.
- While Communicator messaging is ACTIVE, leaving its foreground panel keeps background messaging alive. RadioLab pauses/resumes messaging only when it was active before the RadioLab handoff.
- The two RadioLab user-button positions are physically verified on the M5StickC Plus SE; remaining hardware and radio behavior still requires field verification.
- Communicator v0.1 now provides one-peer availability, a five-message Polish preset catalogue, contextual responses, human-visible OK flow, full-screen incoming cards, short audible notification, foreground/background messaging profile switching, a separate orange `SYGNAŁ` attention action, explicit volatile session enable/disable lifecycle, and a minimal volatile `STANDARD/LR` radio-mode option owned by messaging.
- Communicator v0.1 UI and conversation flow have not yet been physically validated on the two M5StickC Plus SE units.
- Communicator STANDARD/LR switching under the unchanged duty-cycled foreground/background RX schedules still requires two-device hardware validation.
- `SYGNAŁ` LCD layout, bell/arcs animation, ~3.12 s buzzer pattern, audibility, and immediate button-dismiss behavior still require physical device validation.
- A local ESP-IDF build has not yet been executed in the available implementation environment.

## Next planned implementation step

The current near-term implementation plan and checklist is tracked in [next-phase-plan.md](next-phase-plan.md).

- Build Communicator v0.1 core UX with ESP-IDF 5.5.5.
- Verify boot -> splash -> launcher -> RadioLab -> launcher -> RadioLab lifecycle on hardware.
- Flash the same firmware to both M5StickC Plus SE devices.
- Verify LCD behavior with the physically mapped primary/secondary controls.
- Verify buzzer and battery/AXP192 information.
- Verify automatic peer discovery on channel 6.
- Verify PING -> application ACK, local ACK RSSI, peer-reported PING RSSI, and RTT.
- Verify the simplified field screen, HELLO latch/dismiss behavior, and secondary-long mode switching.
- Walk with either unit and verify link freshness loss/recovery.
- Verify NORMAL and Espressif LR separately on physical hardware.

## Known blockers

No architectural blocker is known. Compile-time validation and physical device verification remain pending.

## Chronological progress

Append concise entries here when the authoritative project state changes.

- 2026-09-18 — Established the initial project documentation foundation and recorded the current architecture, decisions, progress, and workflow.
- 2026-09-18 — Added the minimal native ESP-IDF scaffold and M5StickC Plus SE hardware sanity application.
- 2026-09-18 — Implemented the first equal-peer RadioLab foundation for two-device physical ESP-NOW testing.
- 2026-09-19 — Simplified RadioLab for immediate outdoor field testing with direct PING/HELLO controls and incremental screen rendering.
- 2026-09-19 — Added the first static Nikoś OS launcher skeleton and minimal RadioLab start/stop lifecycle.
- 2026-09-19 — Added the first branded shell pass for the boot splash and launcher visuals.
- 2026-09-20 — Added the first Communicator protocol/messaging infrastructure with one-peer presence, application-ACK retry/dedupe, RadioLab transport handoff, and configurable experimental RX schedules.
- 2026-09-20 — Added a lightweight embedded DejaVu Sans subset in the board layer for Polish Communicator UI text preparation.
- 2026-09-20 — Added Communicator v0.1 core UX with fixed launcher integration, Polish preset/response catalogues, deterministic human-OK conversation state, reachability UI, and full-screen incoming cards.
- 2026-09-20 — Added the separate non-blocking Communicator `SYGNAŁ` attention action using existing RING delivery semantics, custom bell/arcs rendering, and a bounded three-repeat buzzer pattern.
- 2026-09-20 — Added explicit Communicator session lifecycle: messaging OFF after boot, volatile launcher-controlled enable/disable, foreground/background lifetime separation, clean session reset, and conditional RadioLab pause/resume.
- 2026-09-20 — Added explicit confirmed whole-device shutdown from the launcher with orderly app/radio cleanup and board-owned M5Unified power-off.
- 2026-09-20 — Added a minimal Communicator `STANDARD/LR` option that switches the existing radio mode through messaging while preserving duty-cycle schedules and delivery state and forcing fresh peer discovery.
- 2026-09-20 — Refined the base Nikoś OS reference palette after physical LCD testing and grouped the launcher header into a compact Polish `NIKOŚ OS` wordmark.
