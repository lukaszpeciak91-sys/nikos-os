# Project Progress

## Current milestone

Communicator v0.1 core UX over the long-lived messaging infrastructure.

## Current state

- Repository architecture and working model defined.
- Native ESP-IDF scaffold implemented for classic ESP32 / ESP32-PICO-D4.
- ESP-IDF 5.5.5, M5Unified 0.2.22, and M5GFX 0.2.29 are pinned.
- Minimal `board`, `radio`, and `protocol` boundaries implemented.
- Boot shows a short deterministic branded signal-synchronization splash and enters the frozen six-item top-level launcher: Komunikator, Narzedzia, Rozrywka, Zegar, Ustawienia, Wylacz. A local four-row viewport keeps the header, battery indicator, and footer clear.
- Narzedzia contains RadioLab, PowerDiag, and Powrot. PowerDiag provides a RAM-only background diagnostic session with two compact pages; leaving its UI does not stop the session. RadioLab and PowerDiag return to Narzedzia after normal exit. Rozrywka remains a Powrot placeholder. Zegar now shows RTC-backed HH:MM (or --:--), USTAW CZAS, MINUTNIK, STOPER, and POWROT. Ustawienia contains Dzwiek, Motyw, Orientacja, and Powrot.
- RadioLab uses the same firmware on both equal peers.
- Versioned DISCOVERY bootstrap, one active peer, PING, application ACK, HELLO, RSSI capture, RTT, recent reachability, and explicit NORMAL/LR selection are implemented.
- User-facing controls consistently use M5 = open/select and BOCZNY = next; internal primary/secondary names remain implementation details. The launcher separately presents Communicator OFF, searching, known/sendable, and recently reachable states from an app_main-owned status snapshot.
- RadioLab controls use secondary short = PING, primary short = HELLO, primary long = NORMAL/LR, and secondary long = return to launcher.
- POWER remains outside launcher/application navigation: short POWER is a system-level display off/wake control, Launcher WYLACZ remains controlled whole-device shutdown, and long POWER remains board/PMIC hardware behavior.
- Display lifecycle remains independent from device/radio state: Active -> Dimmed after ~15 s -> DisplayOff after ~45 s from last meaningful activity. DisplayOff keeps app_main and Communicator background messaging alive. A user-button wake now shows the ~4 s full-screen Clock Glance; the first gesture is consumed, a second fresh gesture dismisses/restores the exact prior UI and is also consumed, and timeout returns directly to DisplayOff. Dimmed input still wakes and performs its normal action.
- DisplayOff now suppresses passive display-only maintenance: hidden Launcher RTC/battery polling and cosmetic redraws stop, passive Communicator status redraw is suppressed, and RadioLab continues processing with rendering/battery sampling disabled. Visible restore refreshes required telemetry; RTC hardware, Communicator/radio semantics, Countdown, Stopwatch monotonic timing, and Active/Dimmed behavior remain unchanged.
- Accepted user-visible Communicator messages and SYGNAL wake the LCD to normal brightness through semantic UI wake points; Presence/ACK/retry/TxResult/reachability traffic does not wake it. No automatic whole-device shutdown or CPU sleep is part of this lifecycle.
- Local Clock v0.1 explicitly initializes the hardware RTC without system-time synchronization, treats VL/read/range failures as invalid (`--:--`), supports manual HH:MM setting with seconds=00/readback verification, and shows cached HH:MM before battery in the main launcher header.
- Countdown Timer v0.1 is a boot-scoped monotonic service independent from RTC. It supports the approved 00:30..15:00 sequence, background Running/Paused state across Launcher/Communicator/RadioLab/Clock Glance/DisplayOff, PAUZA/WZNOW, RESETUJ, retained ZEGAR status, and a pending full-screen KONIEC alert using the selected SYGNAL sound. Communicator foreground traffic has priority without clearing Timer expiration.
- Stopwatch v0.1 is a Launcher-local monotonic session under ZEGAR. It supports START/STOP/WZNOW/RESETUJ/POWROT, displays MM:SS through 99:59, preserves its local state under Clock Glance and Timer alert redraws, and is intentionally discarded on explicit exit or fresh Launcher re-entry; it has no background service, alarm, sound, persistence, RTC coupling, or radio integration.
- Launcher `WYLACZ` provides explicit whole-device shutdown through a default-NIE confirmation; shutdown cleanup is orchestrated by `app_main`, while M5-specific power-off remains inside `board`.
- The board still contains the earlier embedded Polish-font experiment behind a scoped helper, but hardware testing rejected it for normal product UI. Launcher and Communicator now use deterministic native M5GFX Font0 rendering with temporary ASCII-first copy.
- The visual layer supports five boot-scoped hardware-validation themes through semantic display roles: Nikos, Bursztyn, Matrix, Lava, and Noir. All use true/near-black selection surfaces plus a restrained accent marker; Noir deliberately resolves semantic status/attention/danger presentation to monochrome while preserving the same semantic states and non-color cues.
- ESP-NOW callback-owned RX data and metadata are copied into a FreeRTOS queue before callback return.
- A long-lived `messaging::Service` now owns one-peer Communicator presence, reachability, bounded retry/application-ACK delivery, explicit Delivered/Failed outcomes, dedupe, and experimental RX profiles above `radio`.
- Communicator background messaging starts OFF after boot and is explicitly enabled/disabled from the launcher for the current OS session only; the state is not persisted.
- While Communicator messaging is ACTIVE, leaving its foreground panel keeps background messaging alive. RadioLab pauses/resumes messaging only when it was active before the RadioLab handoff.
- The two RadioLab user-button positions are physically verified on the M5StickC Plus SE; remaining hardware and radio behavior still requires field verification.
- Communicator v0.1 is now a latest-wins non-blocking pager: sends never wait for human response, responses are self-contained PresetId + ResponseId messages, the newest received user message replaces older unanswered content, every received preset is skippable, CZEKAC? remains a normal follow-up preset, and delivery ACK remains device-delivery state only. Delivery feedback is a small non-modal WYSYLAM/DOSTARCZONO/NIE DOSTARCZONO banner that can follow ordinary Launcher navigation without waking DisplayOff or obscuring higher-priority UI. SYGNAL, foreground/background profiles, volatile service lifecycle, and STANDARD/LR ownership remain unchanged.
- Settings provides volatile `Lagodny / Klasyczny / Pager` signal-sound selection, a scrollable volatile `Nikos / Bursztyn / Matrix / Lava / Noir` visual-theme selection, and immediate `PRAWA / LEWA` display orientation. Orientation is Board-wide without changing logical button roles. Full reboot restores Lagodny, Nikos, and PRAWA defaults.
- Communicator v0.1 UI and conversation flow have not yet been physically validated on the two M5StickC Plus SE units.
- Communicator STANDARD/LR switching under the unchanged duty-cycled foreground/background RX schedules still requires two-device hardware validation.
- `SYGNAL` LCD layout, bell/arcs animation, ten complete selected-sound playback cycles (~31 s total for the default Gentle sound), audibility, and immediate button-dismiss behavior still require physical device validation.
- Communicator logical delivery now uses experimental 1000 ms + up to 250 ms jitter retries, at most 8 send attempts, and a 12000 ms absolute deadline; delivery metrics are logged for sender power testing.
- TxResult-aware pacing serializes messaging peer unicasts and now also carries one-shot unicast discovery Presence replies. Presence is discovery-oriented: broadcast discovery runs only while no peer is known, known-peer idle state has no recurring Presence TX, and recent-RX age no longer blocks bounded sends. RX duty schedules remain unchanged.
- Communicator protocol v3 uses discriminator 0xA8 and type-specific frames: Presence 2 B, Ring/ACK 6 B, PresetMessage 8 B, and self-contained PresetResponse 10 B carrying logical MessageId + PresetId + ResponseId. v1/v2 compatibility is intentionally not negotiated.
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

## Countdown Timer v0.1 hardware validation

Use [countdown-timer-hardware-test-plan.md](countdown-timer-hardware-test-plan.md) for the required physical A-K validation on the M5StickC Plus SE.

## Stopwatch v0.1 hardware validation

Use [stopwatch-hardware-test-plan.md](stopwatch-hardware-test-plan.md) for the required physical A-I validation on the M5StickC Plus SE.

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
- 2026-09-20 — Froze the base launcher hierarchy as Komunikator / Narzędzia / Rozrywka / Zegar / Ustawienia / Wyłącz, with RadioLab under Narzędzia and explicit visible Powrót rows in every normal submenu.
- 2026-09-20 — Added Settings v1 SYGNAŁ sound selection with boot-default Łagodny and a shared non-blocking player used by both preview and real Communicator SYGNAŁ.
- 2026-09-20 — Added the initial Settings v2 runtime-theme system with centralized compile-time palettes and semantic display roles; later hardware iterations expanded and retuned the identities.

- 2026-09-20 — Bounded Communicator logical delivery with experimental retry jitter/attempt/deadline policy, explicit Delivered/Failed receipts, sender submission metrics, and minimal delivery-failure UI; Presence and TxResult behavior remain unchanged.

- 2026-09-20 — Added serialized messaging-unicast TxResult pacing: MAC SUCCESS enters RX-interval-based application-ACK grace, MAC FAIL/missing result return to bounded retry, pending application ACKs use a fixed small queue, and application ACK remains the only Delivered condition; Presence/protocol/RX policy remain unchanged.

- 2026-09-21 — Changed Communicator Presence from continuous heartbeat to discovery-oriented traffic: unknown peers broadcast at the existing experimental 2000 ms + 0…250 ms cadence, broadcast discovery receives one serialized unicast Presence reply, known idle peers stop periodic Presence, and UI/send eligibility now separates peer identity from recent-RX status.

- 2026-09-21 — Replaced the fixed 20-byte Communicator v1 frame with v2 type-specific application payloads (2/6/6/8/12 B), preserved 32-bit logical IDs and full 16-bit stable semantic catalogue IDs, and moved all send call-sites to the actual encoded length; both sticks must run the same v2 firmware.

- 2026-09-21 — Reworked Communicator for the physical 240×135 LCD: central palettes moved to an experimental near-black foundation, main/response UI now shows one dominant choice at a time, mandatory HumanOk was removed from normal conversation, contextual ZACZEKAĆ? remains, and technical response delivery now restores any single suspended collision context.

- 2026-09-21 — Hardware readability testing rejected the scaled custom Polish UI font for normal product screens. Launcher and Communicator returned to deterministic native M5GFX Font0 rendering with temporary ASCII-first copy; semantic Communicator IDs and all radio/protocol behavior remain unchanged.

- 2026-09-22 — Added display lifecycle v0.1 with experimental 15 s dim / 45 s LCD-off timing, full wake-gesture suppression from DisplayOff, and semantic Communicator wake while leaving messaging/radio timing unchanged.

- 2026-09-22 — Exposed a one-shot DisplayOff physical-button wake reason to app_main as the future Clock Glance extension point; Clock/RTC UI and the approved future ~4 s glance timeout remain unimplemented.

- 2026-09-22 — Polished physical-device copy and confirmations, separated launcher Communicator service/peer status, and expanded the central semantic palette to five distinct hardware-validation themes without changing transport or display-lifecycle policy.
- 2026-09-22 — Added RTC-backed local HH:MM, manual time setting, cached launcher-header time, and the ~4 s Clock Glance with double gesture suppression and immediate Communicator wake priority.
- 2026-09-22 — Added the boot-scoped PRAWA / LEWA display-orientation setting with immediate Board-wide rotation and unchanged M5/BOCZNY button semantics.
- 2026-09-22 — Added background Countdown Timer v0.1 with monotonic deadline timing, pause/resume/reset, background operation through DisplayOff and foreground app changes, one-shot pending expiration, selected-SYGNAL alert reuse, Communicator preemption, and RadioLab render suppression.
- 2026-09-22 — Added Launcher-local Stopwatch v0.1 with monotonic START/STOP/WZNOW/RESETUJ semantics, MM:SS display, explicit session-discarding POWROT, and redraw-safe Clock Glance/Timer overlay behavior without a background service.
- 2026-09-22 — Added a small DisplayOff idle-work power cleanup: hidden Launcher telemetry/render work pauses, passive Communicator redraws are suppressed, and RadioLab keeps processing without hidden rendering/battery polling; no radio timing, CPU sleep, or power-policy change was introduced.
- 2026-09-22 — Added short POWER display toggle v0.1: Active/Dimmed -> DisplayOff and DisplayOff -> normal Clock Glance wake, with full system-level input consumption and no application, Countdown, Stopwatch, Communicator, RadioLab, radio, or shutdown semantics attached to the short click.
- 2026-09-22 — Reworked Communicator into a non-blocking latest-wins pager model: protocol v3 self-contained responses, optional independent human replies, newest-message replacement, skippable incoming presets, safe latest outgoing supersession above serialized TxResult handling, and no modal CZEKAM/DOSTARCZAM/NIE DOSTARCZONO states.
- 2026-09-23 — Polished Communicator latest-wins UX: working message screens now prioritize one large content item without repeated KOMUNIKATOR/WIADOMOSC headings, received responses show small preset context plus dominant response, and latest delivery feedback is serviced outside foreground Communicator input and surfaced non-modally across ordinary Launcher/Communicator UI.
- 2026-09-23 — Added BatteryGuard v0.1: sparse ~10 s Board power sampling, one-shot 20%/10% advisory overlays without DisplayOff wake, hysteresis/re-arm behavior, two-sample <=3300 mV critical confirmation while not charging, and shared controlled shutdown cleanup with Launcher WYLACZ.
- 2026-09-23 — Hardened BatteryGuard v0.1 audit paths: non-positive AXP192 voltage reads are invalid at Board, stale pending advisories cancel on hysteresis recovery, and a visible advisory auto-dismisses when Charging is detected while preserving underlying runtime state.
- 2026-09-23 — Hardware-tuned theme palette v0.2 for the M5StickC Plus SE: darkened all selected surfaces, standardized restrained two-pixel selection markers, differentiated Nikos/Bursztyn/Matrix/Lava palettes, replaced the gray-oriented theme with strict monochrome Noir, and added Noir-only monochrome semantic color resolution at Board.
- 2026-09-23 — Added PowerDiag v0.1: RAM-only monotonic session accounting for LCD/Communicator/RadioLab time, BatteryGuard sample reuse for coherent battery start/current/min/delta data, read-only messaging RX/mode/RSSI observation, and a two-page Tools UI without tasks, persistence, extra PMU polling, or message counters.
- 2026-09-23 — Extended only Communicator `SYGNAL` playback to ten complete cycles of the existing selected sound (~31 s for Gentle), while keeping the shared player, Countdown expiration, and Settings preview at one normal playback per `play_selected()` call.
- 2026-09-24 — Extended PowerDiag v0.1.1 with the latest signed AXP192 battery-current reading from the existing BatteryGuard safety sample; Page 2 now shows diagnostic mA without adding polling or changing BatteryGuard policy.
