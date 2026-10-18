# Project Progress

## Current milestone

Communicator v0.1 core UX over the long-lived messaging infrastructure.

## Mini Racer v0.2 finite race (PR #100)
- Three duration-balanced stages: 7,800 px, 10,100 px, and 10,900 px (28,800 total) lead to a visible finish-line crossing and explicit victory. Traffic spawning and swept collision rules are retained; stage-relative speed scales the existing progression to 85%, 93%, and 100%.
- The prior continuous app-session limit has become a ten-minute **attempt** deadline. A retry starts a fresh attempt; pause, display interruption, and Communicator takeover do not reset its monotonic start. Victory and collision outcomes remain final; there is no automatic restart.
- The shared Entertainment Guard remains unchanged and always has priority. Simulated clean driving with the production speed integrator reaches the finish in approximately 7m15s. Deterministic traffic tests establish an achievable collision-free path, but human-playability and visual fit still require hardware testing.
- Space Defender retains its existing entry-based timer; round/finale changes remain out of scope.

## Entertainment game limits cleanup R0
- Snake has no separate 10-minute continuous-session timeout. Natural collision/game-over, score, restart, warning suspension, and exit behavior remain. The existing central 20-minute Entertainment window / 60-minute break remains the only time admission restriction across games.
- Coin Flip retains its natural single-flip completion, with no gameplay or timing modification in R0.
- Mini Racer still uses `session_started_ms_` and `kSessionLimitMs` (10 minutes counted from entry) and Space Defender still uses `session_started_ms_` with `rules::kSessionMs` (10 minutes counted from entry). These are **not** yet per-round deadlines. Dedicated finale PRs must start a fresh 10-minute *gameplay-round* deadline on each new round, while leaving Entertainment Guard session and cooldown untouched; round failure/retry must never call Guard enter as a new window.
- No changes to charging, communications, power, battery, or Guard persistence.

## Space Defender v0.2 finite mission (PR pending)
- Three finite waves using the existing combat engine, paced at 140 seconds of active simulation each: reconnaissance Scouts; mixed Scouts/Armored; final mixed Scouts/Armored/Shooters. No new enemy systems or background tasks.
- After 420 seconds the spawn gate closes; victory requires every enemy hull and hostile projectile to resolve. Death always overrides victory. The attempt's 600-second monotonic wall-clock deadline includes pause, DisplayOff, and Communicator interruption; retry resets this attempt deadline only.
- The central 20/60-minute Entertainment Guard, charging lifecycle, power and communications remain unchanged. Seeded reachability evidence and hardware testing are required before merging.

## Current state
- System message typography v0.1: system-owned Entertainment break/reserve/near-expiry notices, BatteryGuard advisories/critical shutdown, Timer completion and Charging Lock caption now use the existing PRE-RENDERED Polish UI renderer with Readable/Compact sizing and bounded layouts. Native numeric charging percentages, OK glyphs and clock time remain unchanged; state machines and notice timing are unaffected.

- Repository architecture and working model defined.
- Native ESP-IDF scaffold implemented for classic ESP32 / ESP32-PICO-D4.
- ESP-IDF 5.5.5, M5Unified 0.2.22, and M5GFX 0.2.29 are pinned.
- Minimal `board`, `radio`, and `protocol` boundaries implemented.
- Boot shows a short deterministic branded signal-synchronization splash and enters the frozen six-item top-level launcher: Komunikator, Narzedzia, Rozrywka, Zegar, Ustawienia, Wylacz. A local four-row viewport keeps the header, battery indicator, and footer clear.
- Narzedzia visibly contains Latarka, Miernik halasu, Przelicznik, PowerDiag, and Powrot; RadioLab remains compiled but is not selectable from the launcher to prevent accidental high-power diagnostic sessions. Latarka is a dedicated LCD flashlight utility with 50/75/100% temporary backlight levels, true-white output, and guaranteed restoration of the configured OS brightness on every exit/system takeover. PowerDiag provides a RAM-only background diagnostic session with two compact pages; leaving its UI does not stop the session. Page 2 also reports I NOW plus session I AVG / I N from the same valid ~10 s BatteryGuard current samples, with no additional PMU polling. PowerDiag returns to Narzedzia after normal exit; RadioLab's preserved return behavior is currently unreachable from normal launcher navigation. Rozrywka contains Gry (Snake, Mini Racer, Space Defender, Rzut moneta, Powrot), direct Ciekawostki, and Powrot. Opening Gry does not start Entertainment Guard; actual launches remain guarded. Snake remains the dedicated fixed-memory game. Ciekawostki is a dedicated offline retained-state reader over 600 generated facts (6×100), with LOSUJ across the full catalogue, numeric category title lists, UTF-8-safe two-page-first reading, and no runtime network/persistence. Zegar now shows RTC-backed HH:MM (or --:--), USTAW CZAS, USTAW DATE, KALENDARZ, MINUTNIK, STOPER, and POWROT through a four-row viewport. Ustawienia contains Dzwiek (Melodia / Glosnosc), Jasnosc, Motyw, Orientacja, AUTO WYLACZ., and Powrot.
- RadioLab uses the same firmware on both equal peers.
- Versioned DISCOVERY bootstrap, one active peer, PING, application ACK, HELLO, RSSI capture, RTT, recent reachability, and explicit NORMAL/LR selection are implemented.
- User-facing controls consistently use M5 = open/select and BOCZNY = next; internal primary/secondary names remain implementation details. The launcher separately presents Communicator OFF, searching, known/sendable, and recently reachable states from an app_main-owned status snapshot.
- RadioLab controls use secondary short = PING, primary short = HELLO, primary long = NORMAL/LR, and secondary long = return to launcher.
- POWER remains outside launcher/application navigation: short POWER is a system-level display off/wake control, Launcher WYLACZ remains controlled whole-device shutdown, and long POWER remains board/PMIC hardware behavior.
- Display lifecycle remains independent from device/radio state: Active -> Dimmed after ~15 s -> DisplayOff after ~45 s from last meaningful activity. DisplayOff keeps app_main and Communicator background messaging alive. A user-button wake now shows the ~4 s full-screen Clock Glance; the first gesture is consumed, a second fresh gesture dismisses/restores the exact prior UI and is also consumed, and timeout returns directly to DisplayOff. Dimmed input still wakes and performs its normal action.
- Charging presentation v0.2: a last valid BatteryGuard percentage appears alongside the existing animation below 100%; at voltage-derived 100%, a static filled battery reads 100% until the unchanged three-sample charge-termination rule authorizes OK. Percentage changes do not restart the five-second Charging Lock presentation.
- Charging Mode v0.1 is a VBUS-owned Charging Lock: normal local Launcher/apps/settings/tools stay blocked and M5/BOCZNY/short POWER normally show the charging/full view for 5 s, while incoming PresetMessage/PresetResponse and RING/SYGNAL retain normal Communicator wake/UI and return directly to the lock afterward. The shared incoming drain remains latest-wins and keeps the four-entry transport handoff moving. A hidden M5-BOCZNY-M5-M5-BOCZNY short sequence within 4 s enables a runtime-only Owner Override; 120 s without local button activity relocks automatically. Full criteria are unchanged and Full presentation waits behind incoming/override without losing its one-shot beep. BatteryGuard and radio policy remain unchanged.
- DisplayOff now suppresses passive display-only maintenance: hidden Launcher RTC/battery polling and cosmetic redraws stop, passive Communicator status redraw is suppressed, and RadioLab continues processing with rendering/battery sampling disabled. Visible restore refreshes required telemetry; RTC hardware, Communicator/radio semantics, Countdown, Stopwatch monotonic timing, and Active/Dimmed behavior remain unchanged.
- Accepted user-visible Communicator messages and SYGNAL wake the LCD to normal brightness through semantic UI wake points; Presence/ACK/retry/TxResult/reachability traffic does not wake it. DisplayOff itself does not shut down the device or enter CPU sleep; the separate Auto Power-Off policy performs whole-device shutdown after 20 minutes of eligible local inactivity.
- Local Clock v0.1 explicitly initializes the hardware RTC without system-time synchronization, treats VL/read/range failures as invalid (`--:--`), supports manual HH:MM setting with seconds=00/readback verification, and shows cached HH:MM before battery in the main launcher header.
- Countdown Timer v0.1 is a boot-scoped monotonic service independent from RTC. It supports the approved 00:30..15:00 sequence, background Running/Paused state across Launcher/Communicator/RadioLab/Clock Glance/DisplayOff, PAUZA/WZNOW, RESETUJ, retained ZEGAR status, and a pending full-screen KONIEC alert using the selected SYGNAL sound. Communicator foreground traffic has priority without clearing Timer expiration.
- Stopwatch v0.1 is a Launcher-local monotonic session under ZEGAR. It supports START/STOP/WZNOW/RESETUJ/POWROT, displays MM:SS through 99:59, preserves its local state under Clock Glance and Timer alert redraws, and is intentionally discarded on explicit exit or fresh Launcher re-entry; it has no background service, alarm, sound, persistence, RTC coupling, or radio integration.
- Launcher `WYLACZ` provides explicit whole-device shutdown through a default-NIE confirmation; shutdown cleanup is orchestrated by `app_main`, while M5-specific power-off remains inside `board`.
- The board still keeps the scoped Polish UTF-8 font helper while Launcher and Communicator use deterministic native M5GFX Font0 / ASCII-first copy. Ciekawostki deliberately uses that scoped helper at scale 1 for approved editorial text; the subset now includes the exact additional UTF-8 glyphs present in the 600-fact release dataset.
- The visual layer supports five boot-scoped hardware-validation themes through semantic display roles: Nikos, Bursztyn, Matrix, Lava, and Noir. Theme v0.3 gives the four chromatic themes distinct subtly tinted near-black Background/Surface depth while Noir remains strict neutral black/gray/white. Board now preserves palette values as 16-bit RGB565 through M5GFX calls; the previous uint32_t widening had made M5GFX interpret RGB565 constants as RGB888 and caused the observed blue/cyan cast. Selected rows retain dark Surface plus a restrained two-pixel marker.
- ESP-NOW callback-owned RX data and metadata are copied into a FreeRTOS queue before callback return.
- A long-lived `messaging::Service` now owns one-peer Communicator presence, reachability, bounded retry/application-ACK delivery, explicit Delivered/Failed outcomes, dedupe, and event-driven RX power selection above `radio`: enabled idle is 3000/500 and active/pending logical user delivery temporarily boosts to 1000/500.
- Communicator background messaging starts OFF after boot and is explicitly enabled/disabled from the launcher for the current OS session only; the state is not persisted.
- While Communicator messaging is ACTIVE, leaving its foreground panel keeps background messaging alive. RadioLab pauses/resumes messaging only when it was active before the RadioLab handoff.
- The two RadioLab user-button positions are physically verified on the M5StickC Plus SE; remaining hardware and radio behavior still requires field verification.
- Communicator v0.1 is a latest-wins non-blocking pager with LISTA as the default main send presentation and retained POJEDYNCZO mode over the same selection/send state. Presets plus SYGNAL share one send-action order, OPCJE/POWROT remain fixed after them, response and WaitDecision flows use consistent selectable rows, and MASZ CZAS? displays its first response as TAK without changing ResponseId semantics. Delivery feedback remains application-ACK driven and LISTA associates it with the originating send action. SYGNAL, volatile service lifecycle, STANDARD/LR ownership, and messaging-owned RX policy remain unchanged.
- Settings provides `Lagodny / Klasyczny / Pager` signal selection, persisted 0/25/50/75/100% signal volume, Communicator `LISTA / POJEDYNCZO` view, `Niska / Srednia / Wysoka` brightness, `Nikos / Bursztyn / Matrix / Lava / Noir` theme, `PRAWA / LEWA` orientation, and persisted Auto Power-Off (default ON). Only volume and Auto Power-Off are persisted; the other preferences remain boot-scoped.
- Communicator v0.1 UI and conversation flow have not yet been physically validated on the two M5StickC Plus SE units.
- Communicator STANDARD/LR switching under the unchanged 3000/500 idle and 1000/500 delivery-boost schedules still requires two-device hardware validation.
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
- 2026-09-24 — Added display brightness v0.1 with boot-scoped Niska 72/18, Srednia 96/24 (new default), and Wysoka 128/32 profiles; Board stores/applies Active/Dimmed levels while DisplayLifecycle remains semantic-only. Hardware validation will compare PowerDiag battery current at 72/96/128.
- 2026-09-24 — Decoupled Communicator RX power from UI visibility: enabled idle/open UI now remains BG 3000/500, while messaging temporarily selects FG 1000/500 only for active/pending logical user delivery until Delivered/Failed; PowerDiag reports the effective schedule and peer freshness remains on the stable 20 s enabled-session timeout.
- 2026-09-24 — Hardened messaging-owned RX profile transitions: a failed effective profile change now performs one attribution-safe transport restart with desired-profile recomputation after in-flight cleanup, while restart failure enters explicit fail-closed/faulted messaging instead of leaving an unreconciled high-duty profile.
- 2026-09-24 — Added Communicator list UI v0.1: LISTA is the boot default with a three-row scrolling send-action window and inline SYGNAL, POJEDYNCZO preserves the previous card presentation, delivery feedback retains originating-action context, and response/WaitDecision screens share one selectable-row grammar with context-aware MASZ CZAS? -> TAK display text.
- 2026-09-24 — Fixed Theme Identity v0.3 after physical LCD validation: preserved RGB565 as 16-bit through the Board/M5GFX boundary, eliminating the accidental RGB888 blue/cyan interpretation; added RGB565-aware tinted Background/Surface depth for Nikos/Bursztyn/Matrix/Lava, kept Noir strictly neutral, and confirmed theme switching already performs a complete Theme-screen redraw.
- 2026-09-25 — Added and refined Charging Mode v0.1 into a VBUS-owned Charging Lock: 1 s cable detection, 5 s charging/full presentation, normal incoming Communicator/RING priority with bounded latest-wins queue draining, hidden 4 s owner sequence plus 120 s inactivity relock, deferred one-shot Full presentation behind incoming/override, visible fatal NVS diagnostics after charging boot sleep, and normal-runtime restoration on unplug without changing BatteryGuard or radio/messaging policy.

- 2026-09-25 — Added Snake v0.1 as the first real Entertainment app: 40×20 wrap-around grid over the existing 240×135 landscape orientation, relative M5/BOCZNY steering, fixed 800-cell storage, random unoccupied food, score-driven 220→95 ms movement, RAM-only runtime record, 10-minute per-entry guard, DisplayLifecycle activity on movement ticks, and Communicator preemption with Snake-state resume; no game framework, persistence, radio, or protocol changes.

- 2026-09-26 — Extended PowerDiag for the final A/B battery diagnostic: Running sessions now accumulate signed I AVG and I N from the existing valid/current-supported BatteryGuard samples only; START/NEW TEST resets the statistic, I NOW keeps the existing latest-sample behavior, and no battery-safety or sampling policy changed.

- 2026-09-26 — Added Flashlight v0.1 under Narzedzia: selector-first 50/75/100% temporary backlight levels (128/192/255), full physical white LCD output through Board-only helpers, all M5/BOCZNY actions extinguish the light, POWER keeps unchanged global DisplayOff/Clock Glance ownership, and Communicator/Charging/Timer/Battery takeovers always restore normal brightness and return only to the selector.

- 2026-10-11 — Added Low Battery Reserve v0.1: BatteryGuard exposes a 20%/25% hysteretic Reserve state; app_main resolves the effective profile from Reserve plus the selected brightness and DisplayLifecycle reapplies it without changing Active/Dimmed/DisplayOff state. Reserve uses Low 72/18, charging clears it, and Communicator/radio/critical BatteryGuard behavior remains unchanged.

- 2026-10-11 — Added Auto Power-Off v0.1: a per-device NVS boolean defaults ON and drives controlled whole-device shutdown after 20 minutes without local M5/BOCZNY/short-POWER activity, with VBUS suspension, fresh timing after unplug, Countdown blocking, and incoming-Communicator deferral without changing DisplayLifecycle or radio policy.

- 2026-10-15 — Added an interim Charging Full fallback for sessions that missed a sampled Charging phase; this transitional rule used >=4150 mV, PMIC Discharging, and 0 mA evidence and was superseded by the simplified percent/termination contract below.

- 2026-10-16 — Simplified Charging Full authority to one product-visible contract: three consecutive valid BatteryGuard samples with VBUS present, voltage-derived 100%, and PMIC Discharging latch FULL/OK. Any invalid/unknown/charging/<100%/VBUS-absent sample resets confirmation; battery current remains diagnostic-only.

- 2026-10-11 — Extended the local RTC clock to date/calendar v0.2: Board exposes date-only RTC read/write, ClockService validates 2000-2099 Gregorian dates plus a single persisted rtc_date_configured trust marker required because PCF8563 VL is shared with the seconds register, ZEGAR adds USTAW DATE / KALENDARZ with short-click Calendar actions, and Clock Glance shows DD.MM.YYYY below HH:MM without weekday text or timezone/NTP/Countdown/Stopwatch coupling.

- 2026-10-14 — Completed the authoring-time Facts catalogue with six categories of 100 titled Polish facts each (600 total), refined the content validator, and accepted editorial corrections. The firmware/UI reader, title list, random selection, pagination and input navigation remain a separate implementation iteration.

- 2026-10-14 — Implemented Ciekawostki v0.1 from the merged 600-record Facts catalogue: deterministic source→firmware generation strips source/note, category lists preserve 001..100 plus POWROT, LOSUJ spans all 600 without immediate repeat, the retained reader paginates UTF-8 without heap allocation, and Communicator/system overlays preserve the active Facts state.


## Sound Level Meter v0.1 — implementation candidate

A focused Sound Meter module, relative RMS/peak processor, Board microphone
ownership boundary and Narzędzia runtime entry have been added on an
unmerged feature branch. The existing messaging pause/resume and Countdown
pause/resume contracts are reused, including charging takeover cleanup.
Hardware microphone sampling, display quality, speaker recovery and repeated
enter/exit cycles still require physical validation on M5StickC Plus SE.
No acoustic calibration or dB presentation is included.

### Pre-implementation audit addendum (GO with conditions)

The default Board startup continues to disable the microphone while enabling
its speaker. The optional meter acquires the SPM1423 PDM path only in the
foreground, configures GPIO34 data / GPIO0 clock at tool entry, and releases
the microphone before restoring the speaker. DisplayOff suspends microphone
capture and subsequent visible wake resumes it. Sampling is bounded to 256
16-bit PCM samples at 17 kHz, requested about every 80 ms; the UI has at most
approximately 12.5 updates/sec when values change. The existing display
lifecycle remains authoritative. An already-expired timer blocks entry.

**Unverified for merge:** compile against pinned M5Unified 0.2.22 and ESP-IDF
5.5.5, PDM waveform quality on SE, I2S/audio handoff recovery, long-press
and screen-off interactions, radio reception before and after exclusive mode,
charging interruption and repeated enter/exit. Static review or published
library examples do not constitute hardware confirmation. Do not merge until
CI and physical device checks provide evidence.

## Signal Volume v0.1 — implementation candidate

One focused unmerged PR adds Settings > Sound > Volume alongside the existing
melody selector. Five relative presets (0/25/50/75/100%) are mapped to
M5Unified master-volume 0/16/32/48/64 with the legacy default preserved.
A single NVS preference survives reboot, change failures roll back to the
persisted state, 0% suppresses Board::tone(), and speaker recovery after the
Sound Level Meter reapplies the chosen level. A new build/CI and physical
verification are required: five audible steps, silence, existing melody,
SYGNAŁ, regular preset alerts, countdown, charging tone, reboot, microphone
entry/exit, repeated navigation and volume restoration. No dB or acoustic
linearity claims have been verified.

## Quiz Prawda/Fałsz v0.1 — implementation candidate

Added original approved 120-question JSON as source of truth, deterministic
offline asset generator, separate Quiz component and FactsApp menu integration.
Static dataset validation confirms 120 total, 20 per category, no duplicate IDs,
Polish glyph coverage and five-line question / four-line feedback character
bounds. Host test source has been added; ESP-IDF 5.5.5 build and real 240×135
glyph/display verification have not been run in this execution environment.
The PR must remain unmerged pending green firmware CI and hardware checks for
all screens, buttons, radio preemption, POWER, Charging Lock and battery guards.

### Quiz R1 audit repair

Reordered Ciekawostki to LOSUJ, QUIZ, six categories and POWRÓT. Quiz line breaking now consults actual Polish UI glyph widths via the Board display font; the deterministic validator reads the exact firmware font advance table and checks all approved statements and explanations within their available 226-pixel width. The 120 approved records and generated assets are unchanged; 60 true and 60 false answers were confirmed. Runtime failure shows a visible error instead of silently clipping. Target ESP-IDF build, RAM/flash measurement and physical LCD confirmation remain outstanding.

### Mini Racer v0.1 implementation candidate

A focused, unmerged PR adds Mini Racer under Rozrywka, with two-lane gameplay, scrolling road marks, simple car primitives, automatic bounded difficulty, swept collision detection, pause/game-over actions, a 10-minute session cap and RAM-only score record. The module delegates all system ownership to existing `app_main`, Board and DisplayLifecycle boundaries. Code audit, firmware build, flash/RAM measurement and physical checks of both orientations, repeated entry/exit, incoming Communicator, Charging Lock and BatteryGuard remain required prior to merge.

### Historical: Font Lab v0.1 — temporary experiment
Added an unmerged, independent Tools diagnostic to compare three Polish text
rendering strategies across six identical samples without modifying the
system Unicode font mapper. Included independent fixed bitmap glyphs,
native-letter/diacritic composition and complete pre-rasterized phrase
images. U8g2 is recorded as a researched-but-excluded candidate. The compact
assets are fixed and compiled into firmware. ESP-IDF 5.5.5 compile,
flash/RAM deltas and physical LCD readability/diacritic alignment still
require verification; the application is not a production-wide font change.

### Historical: Font rendering core v0.1 — implementation candidate

A focused unmerged PR introduces a reusable, bounded UTF-8 rendering core
for Font Lab only: unchanged STANDARD fallback, dynamically composed Polish
letters, and offline-generated per-glyph PRE-RENDERED raster text. Removed
obsolete pre-rasterized complete sentence images while preserving six Font
Lab samples and their navigation. Added native C++ host-test source using
a stub Board for glyph metrics, line wrapping, invalid UTF-8, text-fit,
display clipping and default STANDARD behavior. ESP-IDF 5.5.5 compilation,
firmware size delta, stack high-water and LCD visual validation remain
mandatory prior to merge. Main-task stack configuration is untouched
(`CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` already exists on main).

### Font core PR #71 — audit repair (UTF-8, l-stroke, Z-dot)

The decoder's invalid-byte result is now retained by the layout measurer and
`supported=false` is reported for malformed UTF-8 and unsupported codepoints
in both experimental modes. Both still draw a bounded '?' fallback. COMPOSED
draws its L/l stroke inside the narrow lowercase l advance (three plotted
columns, with the next glyph starting after the declared advance). The
PRE-RENDERED uppercase Ż now has a centred dot raster distinct from Ź's acute;
all 18 Polish raster strings were checked for duplicates. The host test covers
these regressions, failed-fit no-draw behavior and explicit pixel bounds.

ESP-IDF 5.5.5 is not installed in the execution environment and the GitHub
repository cannot be cloned from its restricted network, so native C++ tests,
firmware build, linked flash usage, app-partition free bytes, runtime timing
and device validation are not yet confirmed. Per-pixel `Board::fill_rect`
remains a performance concern: up to 192 inspected bits and one drawing call
per set bit per glyph (plus region clearing), to be profiled on target.
`CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` remains unchanged.


### Typography Consolidation v1 — approved hardware baseline (2026-10-17)

The physical M5StickC Plus SE comparison selected **PRE-RENDERED** as the
normal typography for ordinary Nikoś OS application and system UI. The
shared bounded `font_core` renderer remains authoritative; no duplicated
bitmap assets or independent font engine was introduced.

The formerly user-selectable STANDARD / COMPOSED / PRE-RENDERED controls
(`USTAWIENIA > CZCIONKA`) and `NARZĘDZIA > TEST FONTU` were removed.
Font Lab and its resume/action paths are no longer composed into the firmware,
but the comparison sources and host validation tooling remain in the repo.
Product menu headings and selectable labels now use uppercase Polish copy,
including supported accents; prose, Facts data and Quiz data are unchanged.

**Explicit scope exclusions:** Flashlight, Communicator and Charging Lock /
FULL / OK retain their existing presentation, navigation and lifecycle.
The Board text adapter bypasses migration for these foregrounds, as well as
while Charging Mode is active. Compact metadata keeps measured existing
geometry and uses Polish-capable rendering for accented strings when needed.
Facts and Quiz use PRE-RENDERED directly while the later visual and content
polishing stage is deferred. The legacy experiment descriptions above are
historical design notes and are superseded as current product policy by this
section.


### Sound Meter v0.1 — implementation and verification (2026-10-17)

PR #80 R1: removed unnecessary `isRunning()` preflight, added rejected-request
state logs, retained callback/atomic ownership, 600 ms timeout, warmup, constant
PCM detection and bounded min/max/RMS/level logs. Added regression assertions.
Still requires host test execution, ESP-IDF 5.5.5 firmware build, and physical
verification: quiet room → speech → clap → silence → repeated entry/exit →
buzzer at configured volume and restored Communicator. Capture real RMS before
changing signal thresholds. Hardware cause remains provisional until tested.


### Space Defender v0.1 — PR implementation (2026-10-17)

New independent three-lane game, Launcher integration, lifecycle hooks, fixed object pools and C++ host rules test were added. Remaining acceptance requires running host regressions, compiling under ESP-IDF 5.5.5, confirming linked firmware size and visual hardware checks: enemy silhouettes, automatic shots, three hits/1.5 s immunity, scrolling, pause/restart, time limit, charging lock, incoming Communicator and DisplayOff/wake. No flash or merge was authorized.


### Space Defender PR #83 R1 — pending validation

Implemented staged concurrent traffic, candidate arrival-window checks, bounded randomness, clipped lane-band redraw with star restoration, corrected duplicate CMake component registration and expanded C++ host scenarios. The environment cannot clone GitHub or invoke ESP-IDF 5.5.5, so host execution, build and linked firmware size are still unverified. Test hardware for two-button escape timing, late-game hostile shots, sprite overlap/edges, HUD integrity, stars, power/display/charging interruption and Communicator resume before merge.


### Space Defender PR #83 R2 — audit response

Deduplicated `space_defender` in `main/CMakeLists.txt`; root and app registration are single entries. Changed only traffic admission: interval-safe mixed-speed checks across the 500-point boost and no Shooter-hull overlap while future hostile fire is pending. Added production `advance`/`substep` scenarios for moving hulls, relative speeds, warning-to-bullet, surviving bullets, collision/invulnerability, pool bounds and legal cycling. Host execution, ESP-IDF build, device input cadence and high-score reachability remain to be verified.


### Entertainment Guard v0.1 — implementation status

Added a small policy component, narrow NVS record, trusted ClockService civil-minute composition, three protected entry gates, a shared window/break and one-time warning, Reserve interruption, and app_main foreground/presentation coordination. Snake's warning-only pause preserves movement phase; Mini Racer uses its existing suspend/resume, while Facts/Quiz is redrawn without changing state. Existing local 10-minute screens now say KONIEC SESJI / 10 MIN GRY. Host policy tests were added. The exact guard header (source hash verified) passed an isolated g++ C++17 host harness for the 18/20/80-minute boundaries, shared launches, RTC restore, untrusted cooldown and no periodic writes. The full repository host-test suite and ESP-IDF 5.5.5 build remain unavailable, as does physical verification of warning/foreground restoration, charging/Communicator priority, and boot restoration with trusted/untrusted RTC. No merge was authorized.


### Entertainment Guard PR #84 audit iteration — Space Defender and deferred Break

Space Defender now shares guarded entry, warning suspend/resume, 20-minute lifecycle termination, cooldown and authoritative Low Battery Reserve with Snake/Mini Racer/Facts. Its local 10-minute timeout uses neutral KONIEC SESJI / 10 MIN GRY wording. A global expiration while one of these applications is preempted by Communicator ends the retained app and clears its resume flag, while queuing a Break notice without taking foreground; the notice is shown when Communicator exits and dismisses to ROZRYWKA. The existing RTC/NVS cycle timings and non-entertainment ownership remain unchanged. Source regression coverage was extended; full host suite, ESP-IDF build and physical timing tests remain pending.


### Entertainment Guard PR #84 R1 audit repairs

Removed duplicate OpenSpaceDefender enum item, guarded the contiguous Launcher Entertainment action range and added a structural regression requiring all Entertainment launcher routes to match the range and protected runtime. First NVS write failure now aborts admission and rolls back the unpersisted policy start; a failed expiry clear remains latched and is retried only on explicit protected launch. Entertainment warning ownership blocks premature Mini Racer/Space Defender lifecycle resume. The Communicator restoration condition now includes Space Defender. Host regression extended with failure-injection and synthetic Entertainment route coverage; full repository host suite and ESP-IDF build remain unverified until available.


### Entertainment Guard PR #84 R2 audit implementation

R2 replaces enum-range admission with one-shot Launcher Entertainment-origin tagging and explicit recognized-action refusal; unknown tagged routes cannot enter foreground. Mutation-style source regression checks missing origin tagging, recognition, runtime classification, termination, preemption-flag removal and premature warning resume. Guard expiration no longer calls Launcher drawing during Charging Lock or higher-priority overlays, retaining deferred break/Reserve presentation. The policy's trusted/untrusted time and NVS transition model remains unchanged. Host integration and ESP-IDF full firmware build still need execution in a checkout/toolchain-enabled environment.


### Entertainment Guard PR #84 R3 final fixes

Replaced per-action Entertainment tagging with category origin captured centrally in Launcher::update; app_main gates non-None actions from that screen. Unknown actions are refused. Owner Override no longer resets the break-notice rendered flag each loop while it exposes normal UI. Updated source/mutation regression and added permanent AGENTS.md architecture safety contract. Host suite, ESP-IDF compilation and physical Owner Override checks remain unverified pending execution.
