# Project Decisions

This file records concise accepted project decisions. Add new records only when a decision becomes part of the authoritative project state.

## D-001 — Use native ESP-IDF

**Status:** Accepted

Native ESP-IDF is the primary framework.

**Rationale:** It provides direct control over ESP-NOW, Wi-Fi configuration, RX metadata, Espressif LR mode, power management, NVS, and FreeRTOS.

**Current implementation:** ESP-IDF 5.5.5 is pinned exactly.

## D-002 — Use M5Unified/M5GFX for M5 hardware support

**Status:** Accepted

Use M5Unified/M5GFX as ESP-IDF components for initial M5 hardware support.

**Rationale:** Existing maintained hardware support should be preferred over custom LCD or AXP192 drivers unless a real limitation later justifies replacement.

**Current implementation:** M5Unified 0.2.22 and M5GFX 0.2.29 are pinned through the ESP-IDF Component Manager.

## D-003 — RadioLab and Communicator are sibling applications

**Status:** Accepted

RadioLab proves and diagnoses platform capabilities. Communicator v0.1 uses the same lower-level platform services as a sibling foreground application.

**Rationale:** The communicator UI must not define the platform or become a dependency of platform services.

## D-004 — Automatic bootstrap discovery without pairing UX

**Status:** Accepted

RadioLab v0.1 devices periodically broadcast a versioned DISCOVERY packet on a fixed channel. A compatible device may learn one peer MAC from that packet and register it for unicast ESP-NOW traffic.

There is no pairing screen, device list, account/name system, or persistent peer database in RadioLab. Rediscovery after reboot is expected.

**Rationale:** The first field test uses two equal devices and should not require manual MAC entry, while avoiding a general-purpose pairing framework.

## D-005 — Application ACK defines end-to-end delivery

**Status:** Accepted

ESP-NOW send callback success is only a MAC-level result. An application ACK confirms peer-side application processing.

RadioLab uses application ACK for its field diagnostics. Communicator messaging uses a matching ACK reference ID as the completion condition for an outgoing logical message.

**Rationale:** MAC-level send completion and peer-side application processing are different guarantees and must be reported separately.

## D-006 — RSSI remains receiver-side metadata

**Status:** Accepted

RSSI is receiver-side signal metadata. It may be exposed as context but must not be converted into metres or another physical distance estimate.

**Rationale:** RSSI is affected by environment, orientation, obstruction, antenna characteristics, and other variables that make direct distance inference unreliable.

## D-007 — RadioLab v0.1 benchmark radio mode is explicitly selected

**Status:** Accepted

For RadioLab v0.1 and the current benchmark behavior, each benchmark run uses an explicitly selected radio mode: `NORMAL` or `LR`. RadioLab does not automatically switch between NORMAL and LR during that benchmark run.

NORMAL uses the standard ESP32 802.11 b/g/n protocol bitmap. LR uses the Espressif LR-only protocol bitmap.

**Rationale:** Keeping the mode fixed during the current RadioLab v0.1 benchmark preserves measurement validity and comparability. This decision does not establish a permanent platform-wide NORMAL/LR policy.

## D-008 — RadioLab v0.1 devices are equal peers

**Status:** Accepted

Both physical test devices run the same firmware and expose the same RadioLab discovery, PING, ACK, HELLO, and mode-selection behavior.

**Rationale:** Home/carried placement is a test circumstance, not a permanent radio role. Either unit must be usable in either position.

## D-009 — Communicator protocol is separate from RadioLab protocol

**Status:** Accepted

The first Communicator infrastructure uses a separate versioned `communicator_protocol` with PRESENCE, PRESET_MESSAGE, PRESET_RESPONSE, ACK, and RING message types.

**Rationale:** RadioLab v0.1 is a measurement/diagnostic protocol. Communicator delivery semantics are different enough that prematurely generalizing both into one shared protocol would create unnecessary coupling.

## D-010 — Long-lived messaging service owns Communicator delivery semantics

**Status:** Accepted

A small `messaging::Service` lives above `radio` and independently of foreground UI.

For the first implementation it supports one known peer, discovery/recent-RX status, latest peer RSSI, one outstanding outgoing logical message, bounded retry until matching application ACK, and receiver dedupe. Peer identity is distinct from recent-RX age: a known peer remains eligible for bounded delivery after recent-RX status becomes stale, while the logical delivery deadline and attempt budget remain authoritative. Duplicate copies are ACKed again but do not produce duplicate notification events.

Presence is discovery-oriented rather than a continuous liveness heartbeat. Broadcast discovery retains a small bounded configurable jitter only while no peer is known; a received broadcast Presence schedules one one-shot unicast Presence reply.

The service is advanced by the main loop and does not own a separate FreeRTOS task. Its dedupe state is in memory: it survives foreground application changes and RadioLab messaging pause/resume, but not a full device reboot. A sender still retrying across a receiver reboot may therefore cause that logical message to be surfaced again in this first infrastructure version.

**Rationale:** Background communication needs persistent delivery state without tying it to a particular screen or introducing a generic messaging framework.

## D-011 — RadioLab temporarily owns radio during its session

**Status:** Accepted

Long-lived messaging normally owns the active radio transport. Entering RadioLab pauses messaging transport; RadioLab then starts its own continuous-RX session. Exiting RadioLab stops that session and resumes messaging transport.

**Rationale:** This preserves the existing field-test behavior while allowing messaging state to remain alive across foreground application changes.

## D-012 — Messaging RX duty profiles are experimental configuration

**Status:** Partially superseded by D-037

The first messaging foundation provides configurable foreground/background ESP-NOW RX schedules with profile-aware reachability timeouts.

Current experimental starting values are:
- foreground: approximately 1000/500 ms RX schedule with approximately 7000 ms reachability timeout;
- background: approximately 3000/500 ms RX schedule with approximately 20000 ms reachability timeout.

These values are not permanent product or platform policy. D-037 keeps the same numeric schedules but supersedes UI-driven selection and profile-dependent peer-freshness semantics: 3000/500 is the normal enabled-idle schedule and 1000/500 is now a temporary logical-delivery boost.

**Rationale:** Connectionless RX interval/window behavior must be validated on hardware before final background power policy is chosen.


## D-013 — Communicator messaging is session-scoped and OFF after boot

**Status:** Accepted

Communicator background messaging starts OFF after each boot.

The user explicitly enables it from the launcher for the current OS session. The enabled/disabled state is volatile and is not persisted in NVS.

Foreground Communicator visibility is separate from messaging service lifetime:
- leaving the Communicator panel does not disable background messaging;
- explicit `WYŁĄCZ` stops messaging and clears volatile Communicator/messaging session state;
- start -> stop -> start must work cleanly in one OS session.

RadioLab pauses and later resumes messaging only when Communicator messaging was active before RadioLab acquired exclusive radio ownership.

**Rationale:** This gives the user explicit control over background radio activity without coupling service lifetime to a foreground screen or introducing persistence/power-policy infrastructure.


## D-014 — Whole-device shutdown is launcher-requested and board-owned

**Status:** Accepted

The final launcher entry `WYŁĄCZ` requests whole-device shutdown only after an explicit `NIE/TAK` confirmation with `NIE` selected by default.

The launcher owns only the confirmation UI and returns a high-level shutdown action. `app_main` performs orderly runtime cleanup: it stops tones, clears volatile Communicator state, stops messaging, and ensures radio transport cleanup. The board layer then performs hardware shutdown through M5Unified.

The board implementation uses `M5.Power.powerOff()` from pinned M5Unified 0.2.22. Launcher and `app_main` do not access AXP192 directly.

This remains distinct from:
- Communicator `POWRÓT`: close foreground panel only;
- Communicator `WYŁĄCZ`: stop the background Communicator service only;
- launcher `WYŁĄCZ`: power off the whole device.

**Rationale:** Destructive whole-device power control requires explicit confirmation and orderly system cleanup while preserving the existing ownership boundary for M5-specific hardware.


## D-015 — Communicator radio mode is volatile session configuration

**Status:** Accepted

Communicator exposes one minimal radio option:
- `STANDARD` -> `radio::Mode::Normal`
- `LR` -> `radio::Mode::Lr`

The option is applied through `messaging::Service`, never directly from Communicator UI to `radio`.

Default after full boot is STANDARD. The selection is volatile and is not persisted in NVS. It survives foreground Communicator exit/re-entry, RadioLab pause/resume, and Communicator OFF -> ON within the same OS boot.

A successful change uses the existing `RadioService::set_mode()` path without restarting the messaging transport. It updates the messaging mode configuration, clears learned peer registration/reachability/RSSI, and forces fresh PRESENCE discovery while preserving:
- current foreground/background RX profile and schedule;
- presence interval/jitter;
- retry/delivery policy;
- incoming queue;
- receiver dedupe state;
- outstanding logical message and MessageId.

**Rationale:** This allows controlled STANDARD-vs-LR validation under the real duty-cycled Communicator policy while holding messaging timing and delivery semantics constant.


## D-016 — SYGNAŁ sound selection is volatile and shares one player

**Status:** Accepted

Settings v1 introduces the first real Nikoś OS setting: the Communicator `SYGNAŁ` sound.

The selected value lives in a small composition-owned `settings::State` and is one of:
- `Gentle` / `Łagodny` — default after boot;
- `Classic` / `Klasyczny`;
- `Pager`.

The setting is volatile for the current OS boot and is not persisted in NVS.

A small non-blocking `signal_sound::Player` owns the three fixed product patterns. Both Settings preview and received Communicator RING/`SYGNAŁ` playback use the same player and the same definitions. The player is advanced from the normal main loop and delegates actual buzzer hardware calls to `board`.

The ordinary short incoming-message notification tone remains separate and unchanged.

**Rationale:** This establishes one real user preference and one reusable SYGNAŁ playback source without prematurely introducing generic settings persistence, a theme system, or a generic audio/notification framework.


## D-017 — Runtime visual themes extend the existing boot-scoped settings state

**Status:** Accepted

Settings v2 extends the existing composition-owned `settings::State` with one typed visual-theme value:

- `Nikos` / Nikoś — default after boot;
- `Bursztyn`;
- `Matrix`;
- `Lava`;
- `Noir`.

The selection is volatile for the current OS boot and is not persisted in NVS.

Five fixed compile-time palettes live behind a small `ui_theme` boundary. Applications do not know RGB565 values and do not branch on the active theme. They request semantic `board::DisplayColor` roles, and the board layer resolves theme-varying roles through the active palette.

For the current physical-LCD validation pass, Nikos, Bursztyn, Matrix, Lava, and Noir each define meaningfully different values for every normal palette role. These exact palette values remain experimental pending hardware validation and are not frozen as product identity.

Theme-varying roles are background, surface, primary text, secondary text, and visual accent. The chromatic themes retain fixed product-semantic status/attention/error colors. Noir is intentionally strict monochrome: Board maps active/attention/danger to white and inactive to gray while keeping the existing semantic enums and non-color cues intact.

Changing the theme updates `settings::State`, switches the board palette immediately, and redraws the current Theme screen. No Save/Apply step, generic styling engine, per-screen palette, or persistence is introduced.

**Rationale:** The existing boot-scoped settings state is already the correct ownership boundary for real user preferences. A small semantic color cleanup prevents palette-specific names from leaking into applications while keeping runtime theming mechanically simple.


## D-018 — Communicator logical delivery is bounded and measurable

**Status:** Accepted

Communicator logical delivery remains application-ACK based, but retries are no longer indefinite. One outgoing logical message has a configurable experimental retry interval, bounded retry jitter, maximum send-attempt budget, and absolute logical delivery timeout.

The first hardware-test defaults are approximately:
- 1000 ms retry interval;
- up to 250 ms additional retry jitter;
- 8 send attempts;
- 12000 ms absolute logical delivery timeout.

These values are experimental tuning inputs, not permanent product policy.

The first attempt is immediate when transport and a peer identity are available. Retries keep the same logical MessageId. Recent-RX/reachability age does not suppress attempts to a known peer; if that peer is actually unavailable, MAC-result pacing, the attempt budget, application-ACK requirement, and logical timeout produce the finite Failed outcome. Peer silence never resets the deadline or attempt budget. A deliberate `messaging.pause_transport()` handoff to RadioLab is different: while RadioLab intentionally owns the radio, logical delivery timeout and retry-delay clocks are suspended. On resume the same logical MessageId, send-attempt count, and remaining delivery/retry timing budget are preserved. A matching application ACK is the only authoritative `Delivered` outcome. Attempt/deadline exhaustion produces an explicit `Failed` outcome and clears the outgoing logical delivery.

Development metrics record logical delivery kind/outcome, ESP-NOW send-request attempts, immediate send-request failures, logical delivery latency, and cumulative accepted send submissions for logical payloads, application ACKs, and Presence. These are submission-level measurements, not true PHY-level Wi-Fi transmission counts.

This bounded-delivery foundation remains valid after later Presence pacing, protocol revisions, RX duty-cycle schedules, TxResult attribution, and D-031 latest-wins supersession. Under D-031, the currently relevant logical send keeps these bounds; an older user send may intentionally terminate earlier when superseded by a newer one.

**Rationale:** Hardware testing showed that indefinite retransmission can waste sender energy and leave UI state waiting forever. Bounded delivery provides a safe measurement baseline before deeper MAC-aware or Presence optimization.


## D-019 — ESP-NOW TxResult is a serialized pacing signal, not delivery

**Status:** Accepted

`messaging::Service` now consumes ESP-NOW TxResult only for sender pacing and measurement. Application ACK remains the sole authoritative `Delivered` condition.

Messaging peer-unicast submissions are serialized so at most one outgoing logical payload, application ACK, or one-shot unicast Presence reply is awaiting a peer TxResult at a time. Application ACK requests that arrive while this slot is busy are retained in a fixed four-entry pending-ACK queue and are submitted before a due outgoing logical retry when the slot becomes free. Queue overflow is logged; sender retry plus receiver dedupe remains the recovery path.

For an accepted logical payload submission:
- MAC FAIL schedules the existing experimental 1000 ms + 0…250 ms jitter retry;
- MAC SUCCESS does not deliver the logical message; it starts an ACK grace derived from the current local RX interval plus a 250 ms experimental margin, giving approximately 1250 ms in foreground and 3250 ms in background;
- a missing TxResult is guarded for 500 ms. Guard expiry is counted separately and forces a messaging radio transport reset before any newer peer unicast is submitted, preventing a late destination-only callback from being attributed to a newer transmission.

If an application ACK arrives before the payload TxResult is processed, logical delivery completes immediately as `Delivered`, but the physical unicast attribution remains reserved until the matching TxResult or missing-result guard resolves. This prevents the later callback from affecting a newer logical message.

If the missing-TxResult attribution-barrier radio restart itself fails, messaging fails closed rather than behaving like a RadioLab pause: any still-active logical delivery completes as `Failed`, pending unicast/ACK work and stale reachability are cleared, and new logical sends remain rejected until the normal Communicator service lifecycle is restarted with `stop()` followed by a fresh `begin()`. An outgoing message already completed as `Delivered` is not converted to `Failed`.

RadioLab's deliberate transport handoff still freezes logical delivery/retry timing. Any peer unicast whose callback remains unresolved at the handoff is conservatively closed as missing before radio ownership is transferred; the same logical MessageId and attempt count remain, and the resulting retry timing is frozen until resume.

The TxResult pacing rules remain unchanged when Presence becomes discovery-oriented, across protocol revisions, and under D-031 latest-wins replacement. A newer logical send never steals an older in-flight TxResult slot; the existing callback/missing-result attribution barrier resolves first. RX duty schedules, recent-RX timeouts, Wi-Fi power-save mode, and application ACK/dedupe semantics remain unchanged.

**Rationale:** Destination MAC plus success/failure is insufficient to distinguish an outgoing logical payload from an application ACK to the same peer. Serializing only messaging unicast traffic gives deterministic TxResult ownership while allowing MAC success to reduce blind duplicate retransmission without weakening application-level delivery semantics.


## D-020 — Communicator Presence is discovery-oriented

**Status:** Accepted

Communicator Presence is a peer-discovery mechanism, not a continuous liveness heartbeat.

While no peer MAC is known, messaging sends broadcast Presence immediately when the active service begins updating and then at the existing experimental 2000 ms interval plus 0…250 ms jitter. Once a compatible peer is learned, normal periodic broadcast Presence stops and an idle known-peer session produces no recurring Presence traffic.

Peer identity and recent-RX status are separate concepts. Any valid received peer traffic continues to refresh recent-RX/RSSI metadata, but a known peer remains eligible for preset, response, and RING bounded delivery after that recent status becomes stale. The UI uses a neutral known-but-stale state rather than claiming the peer is unavailable.

A valid broadcast Presence learns/confirms the one peer and schedules one lightweight unicast Presence reply through the existing serialized messaging-unicast slot. Application ACK has first priority, the pending Presence reply second, and a due logical payload third. The receiver distinguishes discovery from reply using `radio::RxEvent::destination`: broadcast destination schedules the reply; unicast Presence does not, preventing a Presence-response echo.

A Presence reply has no application-level ACK or retry protocol. Its MAC TxResult is attributed through the same single in-flight slot as other messaging unicasts, and the existing missing-TxResult guard/attribution-barrier recovery applies unchanged.

Communicator OFF clears volatile peer identity. Fresh ON starts discovery again. Radio-mode change clears peer identity and restarts discovery in the selected mode. RadioLab pause/resume preserves learned identity and emits no discovery traffic while transport is intentionally paused. No peer MAC is persisted.

**Rationale:** Communicator usage is sparse and transactional. Continuous heartbeat traffic after discovery spends energy without being required for message delivery; the existing bounded delivery model is the correct mechanism for determining whether a known peer can actually receive a transaction.


## D-021 — Communicator protocol v2 uses compact type-specific frames

**Status:** Superseded by D-031

Communicator protocol v2 replaces the fixed 20-byte v1 application frame with an explicit variable-length type-specific format. Both controlled M5Stick devices must run the same v2 firmware; there is no v1 fallback, negotiation, capability exchange, or compatibility mode.

The common v2 header is two bytes: byte 0 is the fixed v2 discriminator `0xA7`; byte 1 is the existing numeric MessageType. The discriminator itself identifies protocol v2, so version and type are not bit-packed. Unknown discriminator, unknown type, truncated data, or any extra trailing byte causes decode rejection.

Wire sizes are:

| Type | Size |
| --- | ---: |
| Presence | 2 B |
| Ring | 6 B |
| ACK | 6 B |
| PresetMessage | 8 B |
| PresetResponse | 12 B |

Presence carries only the header. Ring carries its 32-bit logical MessageId. ACK carries only the 32-bit logical MessageId being acknowledged and has no independent logical MessageId. PresetMessage carries a 32-bit logical MessageId plus 16-bit PresetId. PresetResponse carries its 32-bit logical MessageId, the 32-bit referenced message ID, and 16-bit ResponseId. All multi-byte IDs remain big-endian.

PresetId and ResponseId are stable 16-bit semantic catalogue identifiers, never UI row/option indexes. The local catalogue remains responsible for mapping IDs to local display text; human-readable strings are never sent over ESP-NOW. Keeping the full uint16_t wire representation avoids an artificial 255-value ceiling and allows catalogue growth without another protocol redesign.

The 32-bit logical MessageId is deliberately retained. This protocol change reduces only the application payload bytes passed to the radio; bounded delivery, application-ACK authority, retry/deadline policy, TxResult pacing/serialization, discovery-oriented Presence behavior, RX schedules, STANDARD/LR behavior, RadioLab ownership, and UI state machines remain unchanged.

**Rationale:** Communicator exchanges predefined semantic IDs, so carrying fields that are unused by a given message type wastes application payload bytes without improving delivery semantics.


## D-022 — Communicator human flow is shallow and technical ACK is not a human OK

**Status:** Superseded by D-031

On the physical 240×135 device, Communicator presents one dominant message/choice at a time. The main screen keeps one linear focus sequence—Preset 1..5, SYGNAŁ, OPCJE, POWRÓT—with secondary short = next and primary short = select. Response choice follows the same NEXT -> SELECT model and shows exactly one selectable response while retaining the received preset for context.

Application ACK remains the sole technical delivery acknowledgement. Normal human conversation no longer emits or waits for a separate mandatory HumanOk response. After sending a normal response, the responder waits for the technical delivery receipt of that response. If it is Delivered, the UI returns to Main unless exactly one `SuspendedWaitingContext` exists from simultaneous-preset collision, in which case that prior WaitingForResponse state is restored automatically. A Failed receipt keeps the existing explicit delivery-failure UX and does not pretend the response succeeded.

For resolved responses, the initiator displays the received response and dismisses it locally without sending another message. For intentionally unresolved responses—`ZA CHWILĘ`, `PÓŹNIEJ`, `SPRAWDZĘ`—the explicit `ZACZEKAĆ?` branch remains because it has conversational meaning. Primary sends the existing Wait preset; secondary closes locally without transmitting anything. The final `TAK, ZACZEKAJ` / `NIE CZEKAJ` response ends the human conversation without a HumanOk.

The deterministic MAC-based simultaneous-preset arbitration and the single suspended-conversation slot remain unchanged in scope. Transport/application ACK, bounded retry, TxResult pacing, Presence discovery, protocol v2, RX schedules, STANDARD/LR, and RadioLab ownership are not redesigned by this decision.

**Rationale:** Physical LCD testing showed that multiple simultaneous rows and a mandatory human OK create visual and conversational overhead on a sparse transactional communicator. Technical delivery and human conversational meaning should remain separate.


## D-023 — Native M5GFX typography and ASCII-first v0.1 copy after hardware readability failure

**Status:** Accepted for v0.1 hardware validation

The embedded custom Polish UI font experiment is not used for normal Launcher or Communicator product UI. Physical M5StickC Plus SE testing showed that the small generated source font became visibly broken/pixelated when scaled, including on ASCII-only words, so normal product UI returns to the native M5GFX `Font0` rendering path.

For this validation phase, interactive product copy is ASCII-first and avoids Polish diacritics. The custom font resource may remain in the board component for isolated experimentation/sanity checks, but normal product screens must not depend on it.

This does not change communication semantics or localization ownership. PresetId and ResponseId remain stable language-independent semantic IDs, human-readable text remains local to each device, and no display strings are sent over ESP-NOW. The ASCII-first copy is a temporary hardware-readability decision rather than a permanent localization architecture.

**Rationale:** On a 240×135 physical LCD, readable native typography is more important than preserving diacritics through a custom font that degrades all text.


## D-024 — Display lifecycle is independent from Communicator/device lifecycle

**Status:** Accepted for hardware validation

Nikoś OS display lifecycle v0.1 has three product states: `Active`, `Dimmed`, and `DisplayOff`. The experimental policy dims after 15 seconds and turns the LCD/backlight off after 45 seconds from the same last meaningful activity timestamp.

`DisplayOff` does not stop the main loop, messaging service, Communicator background reception, or ESP-NOW RX scheduling. The first physical user-button gesture from `DisplayOff` wakes to normal brightness and is consumed until the involved user buttons are released, so the wake gesture cannot also activate UI behavior. A button gesture from `Dimmed` restores normal brightness but continues through normal application handling.

Accepted user-visible Communicator messages and received SYGNAL wake/reset the lifecycle at Communicator semantic acceptance points. Transport-internal Presence, application ACK, retry, TxResult, reachability, and dedupe activity do not count as display activity.

The `power::DisplayLifecycle` component owns only display state/timing/wake-input policy. Its filtered-input result carries a one-shot `WakeReason::UserButton` only for a physical wake from `DisplayOff`; `app_main` consumes that reason as the explicit extension point for the approved future Clock Glance. Normal communication wake through `note_visible_activity()` never produces the user-button reason. `board` owns M5GFX-specific brightness/sleep/wakeup operations, and `app_main` coordinates filtered input with the active foreground application.

Clock Glance itself is not implemented in this decision/PR: there is no RTC, time rendering, HH:MM UI, or glance timer. The approved future behavior is a short (~4 s) Clock Glance that returns directly to `DisplayOff` unless a second fresh user gesture dismisses it; that second gesture will be consumed centrally by the future Clock implementation rather than passed to the previously focused app.

No automatic whole-device shutdown, light/deep sleep, CPU sleep, battery power policy, NVS setting, event bus, or extra task is introduced.

**Rationale:** LCD/backlight savings can be validated independently without risking the already working Communicator transport/session lifecycle.


## D-025 — RTC-backed local clock and top-level Clock Glance v0.1

**Status:** Accepted for hardware validation

Nikoś OS v0.1 initializes the M5StickC Plus SE RTC explicitly through `board` after `M5.begin()` while keeping M5Unified `config.internal_rtc = false`. This avoids the library's automatic system-time synchronization path. RTC initialization must not write/reset the stored time and failure never blocks boot.

`clock::ClockService` owns product-level `HH:MM` validity/read/set behavior. A reading is valid only when the RTC is available, the hardware time read succeeds, the RTC VL/voltage-low indication is clear, and decoded hour/minute/second ranges are valid. No persistent "clock configured" flag is used. Manual set writes `HH:MM:00` and verifies the result by reading the RTC back.

Launcher owns the visible Clock menu/editor and the cached main-header `HH:MM` field. Header sampling is low-rate and only redraws the time region when minute/validity changes; clock redraws do not count as display activity.

Clock Glance is a transient top-level `app_main` UI entered only from `DisplayOff + WakeReason::UserButton`. The first wake gesture remains fully consumed by `DisplayLifecycle`. A second fresh gesture dismisses the glance, is itself suppressed through physical release, and redraws the current Launcher/Communicator/RadioLab state without resetting that application's selection/conversation/session. With no second gesture, the glance returns directly to `DisplayOff` after the experimental ~4 s timeout.

Accepted user-visible Communicator traffic has priority: normal incoming messages and SYGNAL cancel/bypass Clock Glance and render immediately at the existing semantic acceptance point. Radio/messaging ownership, retry/deadline timing, RX profiles, Presence, application ACK, TxResult pacing, MessageId/dedupe, and STANDARD/LR are unchanged.

This clock capability stores the local wall-clock time entered by the user. Date/calendar, timezone, DST, NTP, Internet time, Timer, Stopwatch, alarms, and NVS clock persistence are out of scope.

**Rationale:** The hardware RTC can provide a useful local clock and low-power glance without coupling timekeeping to system time, networking, or the proven Communicator transport lifecycle.

## D-026 — Boot-scoped display orientation without input remapping

**Status:** Accepted for v0.1 hardware validation

Settings exposes `PRAWA` and `LEWA` as a volatile display-orientation preference. Right is the boot default. `board` keeps the M5GFX mapping local: Right selects rotation 1 and Left selects rotation 3. Both rotations retain the normal logical 240×135 landscape drawing surface, so Launcher, Communicator, RadioLab, Clock, Clock Glance, and other normal screens inherit the selected orientation without per-screen branches.

Changing the preference applies it immediately and redraws the complete Orientation screen. It is independent of the active theme and uses the same semantic display colors. No persistence is introduced, so reboot restores Right.

This setting rotates only the display. Physical M5 / BtnA remains the primary action and physical side / BtnB remains the secondary action; user-facing `M5` and `BOCZNY` terminology and all interaction semantics remain unchanged.

**Rationale:** Rotating the whole device should improve left-handed physical use without coupling product orientation to navigation, input mapping, applications, or persistence.

## D-027 — Background Countdown is monotonic and expiration remains pending

**Status:** Accepted for hardware validation

Nikoś OS Countdown v0.1 is a single boot-scoped `countdown::Service` owned by `app_main` and shared with Launcher UI. It is deliberately separate from `clock::ClockService` and RTC wall time. The service uses the monotonic `esp_timer_get_time()` source supplied by composition, stores 64-bit deadline/remaining intervals, and is advanced from the existing main loop. No FreeRTOS task, scheduler/alarm framework, generic event bus, RTC coupling, or NVS persistence is introduced.

The configured sequence is fixed: 00:30 through 05:00 in 30-second steps, then 06:00 through 15:00 in one-minute steps, wrapping to 00:30. Fresh boot defaults to 05:00. Running uses an absolute monotonic deadline; pause captures the remaining interval and resume creates a new deadline, so loop jitter and RTC changes cannot accumulate countdown error. Reset cancels Running/Paused state, clears any expiration, and retains the configured duration for quick restart.

`Expired` is a one-shot pending state rather than a transient audio condition. `app_main` owns the Timer alert overlay across Launcher, Communicator, RadioLab, Clock Glance, Dimmed, and DisplayOff. Initial presentation wakes/resets visible display activity once, cancels Clock Glance, and reuses the existing selected finite `signal_sound::Player` pattern. Audio completion alone never acknowledges expiration. A user dismissal stops Timer audio, acknowledges the pending expiration, suppresses the full physical dismissal gesture through release, and redraws the preserved foreground context.

Accepted user-visible Communicator traffic has higher priority than Timer alert UI/audio. If communication is accepted while Timer expiration is pending or visible, Communicator takes the foreground and the Countdown remains `Expired` until it can be presented after communication priority ends. No generic notification queue is added. RadioLab continues processing with rendering disabled while the Timer overlay is visible and redraws its retained session after dismissal.

Running/paused Timer state by itself does not alter Clock Glance. Only an unacknowledged expiration bypasses/cancels Clock Glance. DisplayOff never pauses Countdown. Explicit whole-device shutdown discards Timer state.

**Rationale:** Countdown timing needs a stable monotonic lifetime independent from wall-clock correctness, while the alert needs explicit acknowledgment and narrow top-level orchestration so it can coexist with the existing display and communication priority rules without creating a general notification subsystem.

## D-028 — Stopwatch is a Launcher-local monotonic session

**Status:** Accepted

STOPER v0.1 is not a second background timing service. Its authoritative state remains inside Launcher as a small `Idle / Running / Stopped` session using monotonic `esp_timer_get_time()` timestamp differences and accumulated elapsed microseconds. It displays `MM:SS`, updates only when the visible whole second changes, and clamps the representable UI at `99:59`.

Clock Glance and the Countdown Timer alert are transient overlays and therefore preserve the current Stopwatch session through normal Launcher redraw. Explicit Stopwatch exit discards the session, and a fresh `Launcher::begin(...)` resets it after foreground transitions such as Communicator. Stopwatch has no alarm, sound, pending event, persistence, RTC coupling, radio behavior, FreeRTOS task, scheduler, or generic timing framework.

**Rationale:** Stopwatch needs precise local elapsed-time measurement but none of Countdown's background ownership or notification semantics. Keeping it Launcher-local preserves the product distinction and avoids adding another long-lived service.

## D-029 — DisplayOff suppresses passive display-only maintenance

**Status:** Accepted for hardware validation

`DisplayState::DisplayOff` remains LCD/backlight state only, not OS suspension. While the LCD is off, `app_main` suppresses Launcher UI updates that would otherwise poll RTC/battery telemetry or redraw hidden Clock/Timer/Stopwatch presentation, while keeping messaging, Countdown expiration detection, signal-sound advancement, Communicator semantic processing, and RadioLab processing alive.

Launcher Communicator status may still update its cached snapshot while hidden but does not redraw the sleeping LCD. RadioLab continues RX/events/timers through `update(..., false)` and skips display-only battery sampling; a visible RadioLab redraw refreshes battery telemetry before presenting retained state. Communicator suppresses only passive main-status redraw while DisplayOff; accepted incoming content and SYGNAL retain their existing semantic wake/render behavior.

Visible Launcher restore continues to use its existing redraw path, which refreshes RTC and battery telemetry before rendering. Clock Glance keeps its independent fresh RTC read. Active/Dimmed timing, wake-gesture suppression, radio/messaging timing, RTC hardware operation, and all transport power policy remain unchanged. No CPU light/deep sleep or new power-management framework is introduced.

**Rationale:** Timestamp- and service-driven semantics do not require hidden LCD maintenance. Removing invisible RTC/I2C/LCD work provides a small pre-hardware-test power cleanup without coupling display inactivity to radio or OS inactivity.

## D-030 — Short POWER controls display visibility only

**Status:** Accepted for hardware validation

The physical POWER button is a system display control, not a third application-navigation button. `Board::poll_input()` exposes only a short `M5.BtnPWR.wasClicked()` event. `power::DisplayLifecycle` consumes that event before normal M5/BOCZNY input: from Active or Dimmed it enters the existing `DisplayOff` state immediately, and from DisplayOff it performs the existing normal wake and reports the same semantic wake reason used to enter Clock Glance.

A short POWER event never reaches Launcher, Communicator, RadioLab, Countdown UI, or Stopwatch controls. It does not acknowledge alerts, exit applications, alter radio/messaging state, or request whole-device shutdown. If Clock Glance is already visible, short POWER returns directly to DisplayOff. A pending Countdown expiration remains pending and reappears on a later wake according to the existing Timer priority rules.

Product controls are therefore: `M5` = application select/open/confirm; `BOCZNY` = application next/back; short `POWER` = display off/wake; Launcher `WYLACZ` = controlled whole-device shutdown. Long POWER remains board/PMIC hardware behavior and is not intercepted, emulated, or assigned a Nikoś OS command.

**Rationale:** Display visibility is a system concern already owned by DisplayLifecycle. Keeping POWER outside application navigation preserves current app state and communication reachability while providing a simple physical LCD toggle without introducing another sleep or power framework.

## D-031 — Communicator is a non-blocking latest-wins pager using protocol v3

**Status:** Accepted for hardware validation

Communicator v0.1 separates device delivery from human conversation. Sending a preset, response, Wait follow-up, or RING never creates a required human-response waiting state or a modal delivery screen. Application ACK still means only that the peer retained the logical message; MAC success remains pacing information and never becomes Delivered.

Protocol v3 uses discriminator `0xA8` and keeps the existing 2-byte header and message-type numbering. Presence is 2 B, Ring and ACK are 6 B, PresetMessage is 8 B, and PresetResponse is 10 B: 2-byte header + 4-byte logical MessageId + 2-byte PresetId + 2-byte ResponseId. Responses are therefore self-contained and are validated with the existing catalogue relation `response_allowed_for(preset, response)`. v2 is not reinterpreted or negotiated; both controlled devices must run v3 together.

The user-message UX is latest-wins. The small messaging queue remains a transport buffer only; Communicator drains retained events and the newest valid PresetMessage/PresetResponse replaces any older unanswered user message. RING/SYGNAL remains a separate attention event and does not erase the retained message context. Every received preset can be skipped locally.

Outgoing user sends also use latest-wins without creating a visible queue. Messaging retains at most one pending replacement. If an older ESP-NOW peer unicast is already awaiting TxResult, its transport attribution object remains intact until that callback or the existing missing-TxResult barrier resolves; only then may the newest replacement become active. Superseded logical operations do not publish stale UI delivery receipts, and Communicator additionally ignores any receipt whose MessageId is not the latest accepted send.

The prior `WaitingForResponse`, `WaitingForWaitResponse`, response-delivery modal state, suspended waiting/collision restoration, expected response reference, and reference-based PresetResponse matching are removed. The contextual `CZEKAC?` action remains, but Wait is just another PresetMessage and its later answer is an ordinary self-contained PresetResponse.

Retry interval/jitter, delivery timeout, attempt budget, application-ACK authority, dedupe, ACK priority, TxResult pacing/serialization, missing-TxResult recovery, discovery-oriented Presence, foreground/background RX schedules, STANDARD/LR behavior, RadioLab ownership, and DisplayLifecycle are unchanged.

**Rationale:** Sparse pager-like communication should stay usable even when a human never answers. Self-contained responses and latest-wins UI remove unnecessary conversation coupling while preserving the proven transport safety and delivery semantics.

## D-032 — Communicator delivery feedback is small, global, and non-modal

**Status:** Accepted for hardware validation

The latest outgoing Communicator logical operation exposes one small informational presentation: `WYSYLAM...` while unresolved, followed by `DOSTARCZONO` or `NIE DOSTARCZONO` for approximately 2.5 seconds. It never becomes a navigation state, never captures M5/BOCZNY, and a newer send immediately owns the status. Existing MessageId filtering continues to prevent stale receipts from replacing the latest send.

Delivery receipts are serviced from the normal `app_main` loop immediately after `messaging.update()`, through a narrow Communicator delivery-service method that does not execute foreground input or incoming-message handling. Launcher receives only a cached presentation snapshot and renders the same small banner across ordinary menu screens. No background task, generic notification center, queue, or persistent status store is introduced.

Display and communication priority remain unchanged. Delivery feedback never calls visible-activity wake, never wakes DisplayOff, and is not rendered over Clock Glance, the pending Countdown alert, incoming Communicator user content, SYGNAL, or RadioLab. A later normal redraw uses the current cached state, so hidden/suppressed feedback cannot overwrite higher-priority UI.

Working Communicator message screens use recovered vertical space for user content instead of repeating `KOMUNIKATOR`, `WIADOMOSC`, `WYBIERZ`, or `DECYZJA` headings. Received responses always show the originating preset as smaller context and the response as the dominant content. The latest-wins message model from D-031 remains unchanged.

**Rationale:** Delivery is useful technical feedback but must not become conversation state. A narrow transient banner preserves confidence in delivery while keeping the 240×135 display focused on the current human message and action.

## D-033 — BatteryGuard uses sparse advisory sampling and confirmed-voltage shutdown

**Status:** Accepted for hardware validation

Nikoś OS uses a tiny system-level `BatteryGuard` above the existing `Board::power_status()` measurement. The guard samples approximately every 10 seconds, including while DisplayOff. This safety sample is separate from Launcher battery telemetry and does not restore hidden Launcher polling or passive DisplayOff redraw work.

Initial hardware-validation thresholds are:

- LOW at battery percentage <= 20%;
- VERY_LOW at battery percentage <= 10%;
- CRITICAL at battery voltage <= 3300 mV while not charging.

LOW and VERY_LOW are advisory overlays only. They never independently wake DisplayOff. VERY_LOW supersedes pending LOW. LOW re-arms only after recovery above 25%, and a still-pending LOW is cancelled at that recovery; VERY_LOW re-arms only after recovery above 15%, and a still-pending VERY_LOW is likewise cancelled. Entering Charging clears pending advisory state, re-arms advisory thresholds, resets any critical-voltage confirmation, and app_main removes an advisory that is already visible before restoring the retained runtime UI.

A non-positive AXP192 voltage read is normalized by Board to an invalid PowerStatus (`voltage_mv=-1`, `level_percent=-1`, `ChargeState::Unknown`). BatteryGuard never treats that sample as advisory or critical evidence and resets any in-progress critical confirmation.

CRITICAL shutdown is not based on percentage and never occurs from one sample. It requires **two valid low-voltage samples approximately 10 seconds apart**, each <= 3300 mV while not charging. A higher/invalid voltage or Charging resets confirmation. Once confirmed, app_main wakes the display, replaces ordinary overlays with `NISKA BATERIA / WYLACZAM...` for about 1.75 seconds, then uses the same narrow controlled-shutdown helper as Launcher `WYLACZ`: stop sound, reset Communicator session, stop messaging, stop radio, and call `Board::power_off()`.

Advisory presentation priority is below accepted Communicator traffic/SYGNAL and Countdown expiration, but above Clock Glance/ordinary UI. Advisory dismissal consumes the M5/BOCZNY gesture and restores the retained runtime UI. POWER remains the system display off/wake control and does not acknowledge battery warnings.

The 20%, 10%, and 3300 mV values are initial hardware-validation values, not permanent calibrated battery truth.

**Rationale:** Sparse sampling adds negligible overhead while providing advisory UX and a confirmed-voltage controlled shutdown before very deep discharge, without creating a general power-management framework.

## D-034 — Theme v0.2 uses dark selection surfaces and hardware-distinct identities

**Status:** Accepted for hardware validation

Physical testing on the M5StickC Plus SE showed that the earlier theme candidates were technically different but visually converged because bright selected surfaces occupied too much of the 240×135 UI. Theme v0.2 therefore keeps selected surfaces very close to black and uses high-contrast text plus a restrained two-pixel accent marker as the common selection language. Confirmation screens may keep their existing compact `>` marker. Layout, navigation, button semantics, and Font0 typography remain unchanged.

The five runtime identities are:

- Nikos: black / warm ivory / restrained mint;
- Bursztyn: black / amber-gold;
- Matrix: black / green monochrome-terminal identity;
- Lava: black / warm light text / ember orange-red;
- Noir: strict black / white / gray.

Noir replaces the previous gray-oriented Graphite identity and intentionally remaps normal semantic status, attention, and danger presentation to monochrome at the Board color-resolution boundary. Applications continue requesting the same semantic roles and do not branch on theme. Meaning remains available through text, marker shape, filled/hollow treatment, and brightness hierarchy rather than hue alone.

All five palettes remain compile-time RGB565 constants behind `ui_theme`; no framebuffer/color-depth change, generic styling engine, theme-specific navigation, or runtime RGB editor is introduced.

**Rationale:** On a small ST7789V2 panel, theme identity must survive normal viewing distance. Dark selected surfaces stop selection from visually overwhelming the palette, while centralized RGB565 roles keep the implementation small and auditable.

## D-035 — PowerDiag is a volatile background diagnostic session, not telemetry infrastructure

**Status:** Accepted for hardware validation

PowerDiag v0.1 adds two narrow layers. `PowerDiagSession` owns RAM-only measurement state and monotonic 64-bit accumulators; `PowerDiagApp` owns the two-page UI and START/NEW TEST navigation. A Running session survives leaving the PowerDiag UI and continues through Launcher, DisplayOff, Clock Glance, Communicator, Countdown, Stopwatch, RadioLab, and advisory overlays. Reboot or whole-device power-off naturally clears it.

app_main supplies semantic observations rather than allowing the session to query Launcher, CommunicatorApp, or RadioLabApp internals. The session accumulates total time, LCD Active/Dimmed/Off time, Communicator-enabled time, Communicator-foreground time, and RadioLab-foreground time from `esp_timer_get_time()` timestamp deltas.

PowerDiag reuses BatteryGuard's existing approximately-10-second `Board::power_status()` safety sample. BatteryGuard returns the same sampled `PowerStatus` in `UpdateResult`; no second PowerDiag PMU poll is introduced. Valid samples retain battery start/current/minimum voltage, start/current percentage, signed voltage delta, latest charge state, and the latest supported AXP192 signed battery-current reading. Battery current is diagnostic only: 0 mA remains a legitimate neutral reading, no FULL/error heuristic is inferred from it, and BatteryGuard does not consume current for advisory or shutdown decisions. Invalid voltage samples do not affect battery baseline/minimum calculations.

Messaging exposes only one new observation accessor, `current_rx_schedule()`, returning the already-configured current FG/BG schedule. PowerDiag may display Communicator ON/OFF, RX profile/interval/window, radio mode, peer known/reachable state, and latest valid RSSI, but does not change transport configuration.

No NVS, flash logging, FreeRTOS task, event bus, scheduler, generic telemetry/logging framework, battery history, or user-message counters are introduced in v0.1.

**Rationale:** The first hardware power comparisons need enough retained context to explain battery/runtime differences without changing the behavior being measured.

## D-036 — Display brightness is a boot-scoped three-level setting

**Status:** Accepted for hardware validation

Settings adds one semantic display-brightness preference:

- `Low` / NISKA: Active 72, Dimmed 18;
- `Medium` / SREDNIA: Active 96, Dimmed 24;
- `High` / WYSOKA: Active 128, Dimmed 32.

Medium 96/24 is the new boot default. The exact three profile mappings are centralized behind `settings::brightness_profile()`. `settings::State` stores only the semantic selection; Board stores the currently applied Active/Dimmed numeric levels and uses them for immediate Active preview, normal dimming, and wake. DisplayOff behavior is unchanged.

`power::DisplayLifecycle` remains the sole semantic lifecycle owner for Active / Dimmed / DisplayOff and contains no brightness-level policy or numeric brightness constants. Clock Glance inherits the normal Board Active brightness through the existing wake path.

The brightness preference is volatile and returns to Medium after reboot. No NVS, Preferences, flash persistence, slider, brightness service, or raw 0..255 user control is introduced.

Hardware validation will compare PowerDiag battery current and LCD ON time at Active brightness 72, 96, and 128 under otherwise similar runtime/radio conditions.

**Rationale:** Three fixed semantic levels are sufficient for physical readability/power testing while preserving the existing display-lifecycle and Board ownership boundaries.

## D-037 — Communicator RX boost follows logical delivery, not UI visibility

**Status:** Accepted for hardware validation

Communicator-enabled idle RX uses the existing background schedule at all normal UI/display states:

- idle / normal enabled: 3000 ms interval / 500 ms wake window;
- temporary logical-delivery boost: 1000 ms interval / 500 ms wake window.

The temporary boost is owned internally by `messaging::Service`. It begins when a user logical send (PresetMessage, PresetResponse, or Ring/SYGNAL) is accepted and remains active through initial send, TxResult pacing, application-ACK wait, retry delays/retries, bounded deadline, and any latest-wins pending replacement. It ends when the final active/pending logical delivery resolves Delivered or Failed.

Communicator foreground visibility, DisplayLifecycle state, incoming-message presentation, received SYGNAL playback, Presence traffic, and automatic application-ACK transmission do not request the boost. `CommunicatorApp::begin()/end()` therefore no longer own RX profile selection.

The existing config values remain unchanged: the fast profile retains its configured 7000 ms reachability field and the idle profile retains 20000 ms. However, user-visible `peer_reachable()` freshness stays on the stable 20000 ms enabled-session timeout so a temporary delivery boost cannot make an otherwise fresh peer appear stale.

RadioLab still pauses messaging transport and freezes logical delivery clocks. If a logical delivery remains active/pending, messaging retains the desired boosted state while paused and resumes transport directly at 1000/500. No temporary 3000/500 gap is introduced by latest-wins replacement or RadioLab resume.

PowerDiag remains observation-only and reports the actual effective profile/schedule: BG 3000/500 while idle (including open Communicator UI), FG 1000/500 while logical delivery is active/pending, then BG after Delivered/Failed.

**Rationale:** Faster RX is needed to improve application-ACK delivery behavior, not to reward screen visibility with higher radio duty cycle. Event-driven ownership in messaging avoids continuous profile reconfiguration from the main loop and reduces enabled-idle power cost without changing delivery authority, retry, TxResult, dedupe, Presence, or radio-ownership semantics.
