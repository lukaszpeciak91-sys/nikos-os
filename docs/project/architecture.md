# Architecture

## High-level direction

Nikoś OS is a small modular embedded platform. It is not firmware dedicated only to the Nikoś Communicator.

The conceptual model is:

`BOOT -> PLATFORM / CORE -> lightweight launcher -> applications`

Applications may initially be compiled into a single firmware image. No dynamic APK-style or plugin system is required.

The current runtime starts with a lightweight launcher. Its frozen base top-level hierarchy is: `Komunikator`, `Narzędzia`, `Rozrywka`, `Zegar`, `Ustawienia`, and final whole-device `Wyłącz`. The launcher keeps a small local four-row viewport so the six fixed entries do not overlap the header/footer.

The hierarchy is deliberately shallow and explicit:
- `Komunikator` owns the existing communication lifecycle entry/enable UI.
- `Narzędzia` contains `RadioLab`, `PowerDiag`, and visible `Powrót`; RadioLab and PowerDiag remain application siblings launched through the tools category.
- `Rozrywka` remains a placeholder with visible `Powrót`. `Zegar` exposes the RTC-backed current `HH:MM`, `USTAW CZAS`, background `MINUTNIK`, local-session `STOPER`, and visible `POWROT`. `Ustawienia` owns the existing sound, theme, and display-orientation choices.
- `Wyłącz` remains whole-device shutdown with explicit confirmation.

This remains fixed launcher screen/state handling, not a generic menu tree, navigation stack, dynamic registry, filesystem-like folder model, or plugin framework.

## Initial ownership boundaries

### board

Owns M5-specific hardware integration:

- LCD
- buttons
- buzzer
- AXP192 / PMU
- battery information
- display wake/activation
- M5-specific hardware integration

### clock

Owns the small product-level local wall-clock capability above `board`:

- trustworthy `HH:MM` read semantics;
- RTC validity derived from hardware availability, successful read, valid ranges, and clear RTC voltage-low/VL indication;
- manual `HH:MM` setting with seconds forced to `00`;
- write verification through RTC readback;
- lightweight `HH:MM` formatting with `--:--` for invalid/unavailable time.

`board` owns the M5Unified/PCF8563 hardware calls. RTC initialization is explicit after `M5.begin()` while `config.internal_rtc` remains false, avoiding M5Unified's automatic `setSystemTimeFromRtc()` path. Initialization does not write/reset time registers and RTC failure does not block boot.

The v0.1 clock service is local wall-clock time only. It has no date/calendar model, timezone, DST, NTP, system-time synchronization, Timer/Stopwatch timing ownership, alarm clock, or NVS clock-configured flag.

### radio

Owns transport-facing Wi-Fi / ESP-NOW integration and radio hardware behavior:

- Wi-Fi / ESP-NOW initialization
- peer registration
- Wi-Fi channel configuration
- NORMAL / Espressif LR mode
- raw broadcast and unicast TX/RX
- RX radio metadata
- MAC-level send result
- configurable ESP-NOW connectionless RX wake interval/window behavior

The radio layer exposes RX power behavior as transport configuration. It does not decide permanent product power policy.

ESP-NOW callbacks perform only bounded copying into a queue. Application and messaging logic executes later in normal task context.

### protocol

The existing `protocol` component owns the versioned RadioLab v0.1 wire format:

- identifiable on-air format for RadioLab v0.1
- RadioLab message types
- identifiers and sequence information
- explicit encode/decode responsibilities

This remains RadioLab-specific. It is not shared Communicator infrastructure.

### communicator_protocol

Owns the versioned Communicator wire format. Current Communicator traffic uses protocol v3 only; earlier v1/v2 compatibility or negotiation is intentionally not implemented because both controlled devices are flashed together.

The v3 common header is exactly two bytes:

- byte 0: fixed v3 discriminator `0xA8`;
- byte 1: the existing `MessageType` value (`1..5`).

All multi-byte IDs are encoded explicitly in big-endian order. No packed C++ structs, reserved bytes, serializer framework, or dynamic allocation are used.

| Message type | v3 payload layout | Size |
| --- | --- | ---: |
| Presence | 2 B header | 2 B |
| Ring / SYGNAŁ | 2 B header + 4 B logical MessageId | 6 B |
| ACK | 2 B header + 4 B referenced logical MessageId | 6 B |
| PresetMessage | 2 B header + 4 B logical MessageId + 2 B PresetId | 8 B |
| PresetResponse | 2 B header + 4 B logical MessageId + 2 B PresetId + 2 B ResponseId | 10 B |

The 32-bit logical MessageId remains the retry/dedupe and ACK identity. PresetResponse is self-contained: PresetId plus ResponseId is sufficient to validate and render the human response without remembering the originating outgoing MessageId. The local catalogue validates the pair with `response_allowed_for(preset, response)`; ResponseId alone is never used to infer context.

Human-readable preset/response text is never transmitted over ESP-NOW. This component remains intentionally separate from RadioLab protocol.

### messaging

`messaging::Service` is the long-lived Communicator transport/delivery service above `radio`.

It currently owns:

- one known peer slot
- one-peer discovery Presence state, distinct peer identity, and recent-RX/reachability status
- latest peer RX RSSI
- stable logical message IDs across retries
- one active outgoing logical delivery plus one latest-wins pending replacement slot; the pending slot is not a user-visible queue
- bounded retry-until-application-ACK delivery with a fixed logical deadline, configured attempt budget, and bounded retry jitter
- explicit Delivered/Failed logical delivery outcomes with attempt/latency instrumentation
- one serialized messaging-unicast slot so outgoing logical payloads and application ACKs cannot have ambiguous peer-MAC-only TxResult attribution
- a fixed four-entry pending application-ACK queue; queued ACKs have priority when the messaging unicast slot becomes free
- TxResult-aware pacing: MAC success waits for the application ACK using the current local RX interval plus an experimental margin, while MAC failure/missing result returns to bounded retry pacing
- a configurable missing-TxResult guard; guard expiry clears attribution through a messaging radio transport reset before a newer unicast is allowed
- bounded logical delivery may target a known peer even after recent-RX status becomes stale; recent reachability is diagnostic/status information rather than a send-permission gate
- deliberate RadioLab transport ownership pauses suspend logical delivery timeout and retry-delay clocks; resume preserves the same logical MessageId, attempt count, and remaining delivery/retry budget
- receiver-side in-memory dedupe
- duplicate ACK behavior without duplicate notification
- discovery-oriented broadcast Presence with bounded jitter while no peer is known, plus one-shot serialized unicast Presence reply to received broadcast discovery
- a small volatile incoming transport buffer exposed through peek/consume; Communicator drains it and retains only the newest valid user-visible message, so it is not an inbox/history
- delivery completion receipts for the currently relevant logical operation; superseded operations do not produce stale UI receipts
- foreground/background experimental RX profile selection, including profile-aware reachability timeout

The service has no dedicated FreeRTOS task. It is advanced from the normal main loop. Communicator retry interval/jitter, maximum send attempts, and logical delivery timeout are experimental configuration for hardware tuning rather than permanent product policy. Application ACK remains the only authoritative Delivered condition. ESP-NOW TxResult is consumed only as a pacing/measurement signal: MAC success lengthens the ACK wait, MAC failure schedules the existing bounded retry, and a missing callback has bounded recovery. Presence is discovery-oriented: while no peer is known, broadcast discovery uses the experimental 2000 ms + 0…250 ms cadence; once the peer is known, normal idle operation stops periodic Presence TX. A received broadcast Presence schedules one serialized unicast Presence reply, while a received unicast Presence never triggers another reply.

Messaging service lifetime is independent of foreground Communicator UI. Background Communicator messaging starts OFF after boot and is enabled explicitly for the current OS session only. This enabled/disabled state is volatile and is not persisted in NVS.

While enabled, leaving the foreground Communicator UI does not stop messaging; it restores the background RX profile. Explicitly disabling Communicator stops the messaging transport and clears volatile messaging/session state.

While RadioLab owns the radio for its field-test session, messaging transport is paused only if Communicator messaging was active before the handoff. RadioLab exit resumes messaging only in that case; it must not start an OFF Communicator session.

Receiver dedupe state is part of that long-lived in-memory service state, so it survives foreground application changes and the RadioLab pause/resume handoff. It is intentionally volatile across a full device reboot. In this first infrastructure version, if a sender is still retrying an outstanding logical message when the receiver reboots, that message may be surfaced again after the receiver restarts.

### countdown

`countdown::Service` owns one boot-scoped background countdown independent from RTC and wall-clock `HH:MM`. `app_main` supplies the monotonic `esp_timer_get_time()` source, owns the service lifetime, and advances it from every normal main-loop iteration; there is no countdown task, scheduler, alarm framework, event bus, or persistence.

The service owns the configured duration plus the explicit `Idle / Running / Paused / Expired` state. Running time is deadline-based using 64-bit monotonic microseconds, so ordinary loop jitter does not accumulate error. Pause captures the remaining interval; resume establishes a new monotonic deadline from that retained interval. The approved setup sequence is 30-second steps from 00:30 through 05:00, then one-minute steps through 15:00, wrapping to 00:30. Reset returns to Idle while retaining the configured duration.

Launcher owns only the Timer setup/active UI and actions. The service remains authoritative when Launcher, Communicator, RadioLab, Clock Glance, Dimmed, or DisplayOff is active. Running/paused Timer state does not replace normal Clock Glance behavior.

Expiration transitions once to `Expired` and remains a pending user-visible event until explicit acknowledgment. `app_main` owns the full-screen Timer alert overlay, wakes the display once when presenting it, cancels Clock Glance, and reuses the selected `signal_sound::Player` pattern. Audio completion never acknowledges the event. Accepted Communicator traffic has higher priority and may preempt the Timer overlay without clearing `Expired`; the pending Timer is shown after the communication foreground priority ends. RadioLab continues its normal processing with rendering suppressed while the Timer overlay is visible. No Timer state survives whole-device shutdown.

### launcher-local stopwatch

Stopwatch v0.1 is intentionally local to the Launcher `STOPER` screen rather than an `app_main`-owned background service. Launcher retains only `Idle / Running / Stopped` session state, the current-run monotonic start timestamp, and accumulated elapsed microseconds. Elapsed time is derived from `esp_timer_get_time()` timestamp differences so main-loop jitter does not accumulate measurement error.

The visible value is `MM:SS`, updated only when the displayed whole second changes and clamped at `99:59`. Stopwatch redraws do not create display activity, so normal Dimmed/DisplayOff behavior remains unchanged. Clock Glance and the Countdown alert may temporarily cover STOPER; `launcher.redraw()` preserves and recomputes the local session. Explicit `POWROT`/BOCZNY exit discards the session, and a fresh `Launcher::begin(...)` also resets it after foreground transitions such as Communicator.

Stopwatch has no task, `app_main` service, alarm, sound, pending notification, persistence, NVS state, RTC dependency, RadioLab integration, or networking behavior.

### settings

`settings::State` owns the current boot-scoped user preference state. It now contains three real runtime preferences:

- Communicator `SYGNAŁ` sound: Gentle / `Łagodny` (default), Classic / `Klasyczny`, or Pager;
- visual theme: Nikos (default), Bursztyn, Matrix, Lava, or Noir;
- display orientation: Right / `PRAWA` (default) or Left / `LEWA`.

The state is created by composition in `app_main`, is shared with the launcher Settings UI and the narrow consumers that need each typed value, and is intentionally volatile across reboot. No NVS or persistent settings schema is introduced yet.

### ui_theme

`ui_theme` owns five fixed compile-time visual palettes in runtime order: Nikos, Bursztyn, Matrix, Lava, and Noir. The v0.2 values are hardware-tuned for the 1.14-inch ST7789V2 panel: all backgrounds are true black, all selected surfaces remain close to black, and identity comes primarily from primary/secondary typography plus a restrained accent marker rather than large bright fills. The values remain hardware-validation candidates rather than permanent product identity.

All current and future normal screens, including Clock and Clock Glance, request semantic `DisplayColor` roles through `Board` rather than hard-coded RGB values or direct M5GFX color calls. Applications do not contain per-theme branches. Thus normal screens inherit the selected palette automatically. Theme-varying roles are `Background`, `Surface`, `PrimaryText`, `SecondaryText`, and `Accent`. Normal chromatic themes retain the existing fixed product-semantic status/attention/danger colors. Noir is the deliberate exception: Board remaps `StatusActive`, `StatusInactive`, `Attention`, and `Danger` to white/gray palette values while semantic state remains unchanged. Existing text, symbols, filled/hollow markers, and shape differences continue to carry meaning, so Noir does not depend on hue alone.

The active palette is held by `board` and selected from `settings::State::theme`. No generic styling engine, per-screen palette, or runtime RGB editor is introduced.

Display orientation is independent of the active palette. `board` owns the hardware mapping: Right uses M5GFX rotation 1 and Left uses rotation 3, while normal screens continue drawing in logical 240×135 coordinates and inherit the active Board orientation automatically. The logical button roles do not change: physical M5 / BtnA remains primary and physical side / BtnB remains secondary. Orientation is boot-scoped and returns to Right after reboot.

For the v0.1 physical-LCD readability pass, normal Launcher and Communicator product UI uses the native/default M5GFX `Font0` path through `board::draw_text_region()`. The small custom Polish-font experiment remains isolated behind its explicit helper but is not used by normal product UI because real-device testing showed unacceptable readability even for ASCII text when that small source font was scaled. Interactive v0.1 copy is temporarily ASCII-first and rewrites ambiguous words rather than mechanically removing diacritics. User-facing controls are named `M5` and `BOCZNY`; internal primary/secondary names remain implementation details. This is a hardware-driven readability choice, not a permanent localization architecture. Communicator wire semantics remain language-independent: stable PresetId/ResponseId values are transmitted, never display strings.

### signal_sound

`signal_sound::Player` owns the three fixed Communicator `SYGNAŁ` buzzer patterns and their non-blocking playback state.

It:
- reads the selected `settings::SignalSound`;
- starts one complete fixed pattern;
- advances from the normal main loop;
- stops immediately on request;
- uses only `board::tone()` / `board::stop_tone()` for buzzer hardware.

Settings preview and received Communicator `SYGNAŁ` both use this same player and the same pattern definitions. It is not a generic audio engine, arbitrary sequencer, notification framework, or FreeRTOS audio task.

### storage

Owns versioned persistent configuration when persistence is required.

No persistent contact database, chat history, or user-settings schema is introduced by the current implementation.

### power

Owns the first small product-level display lifecycle policy only:

- `Active`, `Dimmed`, and `DisplayOff` state;
- experimental inactivity timing: dim after 15 s and LCD off after 45 s from the last meaningful visible activity;
- wake/activity accounting;
- full wake-gesture consumption after `DisplayOff`;
- a one-shot `WakeReason::UserButton` result when a physical user button wakes `DisplayOff`, exposed with the centrally filtered input for `app_main`.

`board` remains the hardware owner for normal brightness, dim brightness, LCD sleep, wake, and RTC hardware access. `app_main` coordinates the single filtered user input with the current foreground application. Accepted user-visible Communicator messages and received SYGNAL wake through the product lifecycle at Communicator semantic acceptance points, not from radio/protocol callbacks. These semantic communication wakes do not produce `WakeReason::UserButton` and take priority over Clock Glance. A physical `DisplayOff` user wake now enters the top-level Clock Glance transient UI in `app_main`; no second application action leaks from either the first wake gesture or the second glance-dismiss gesture.

`DisplayOff` is LCD/backlight state only. It does not stop `app_main`, messaging, Communicator background reception, or the configured ESP-NOW RX schedule, and it never performs whole-device shutdown.

Whole-device shutdown remains separate: the launcher emits only an explicit shutdown request, `app_main` performs orderly runtime cleanup, and the board layer owns the M5-specific `M5.Power.powerOff()` call.

The current messaging RX schedules remain experimental transport configuration and are unchanged by display lifecycle v0.1. No automatic device shutdown, CPU sleep, radio sleep policy, or persistent power setting is introduced.

### launcher

Owns the current fixed launcher navigation, its explicit shallow category screens, and the local-session Stopwatch presentation/state.

It does not own radio lifecycle internals, application registries, persistence, profiles, plugin loading, or hardware power control. The launcher may request whole-device shutdown only after explicit confirmation. RadioLab is launched from `Narzędzia`; after RadioLab exits, the launcher returns to `Narzędzia` rather than MAIN.

### applications

Current and future applications include:

- Communicator v0.1
- RadioLab
- future diagnostic, Wi-Fi, BLE, IR, and hardware tools

RadioLab v0.1 uses equal peers running the same firmware. It does not assign permanent BASE/MOBILE roles.

Communicator is a foreground UI over the session-scoped `messaging::Service`. Enabling Communicator starts the service for the current OS session. Entering the foreground panel selects the experimental foreground messaging RX profile; exiting restores the background profile without disabling the service. Delivery/retry work remains independent from foreground UI state.

Communicator v0.1 uses a latest-wins, non-blocking pager model. Sending a preset, response, Wait follow-up, or SYGNAŁ does not create a modal delivery/conversation state. Human responses are optional independent messages. The UX retains only the newest valid received PresetMessage/PresetResponse; a newer one replaces an unanswered older one. The messaging queue remains only a small transport buffer, not an inbox/history. RING/SYGNAL is a separate attention overlay and does not erase the retained current user-message context.

The physical 240×135 UI presents one primary message/choice at a time. Main navigation remains five presets, SYGNAŁ, OPCJE, POWRÓT with secondary short = next and primary short = select. Every received preset can be skipped with secondary short; response selection cycles with secondary short and sends with primary short.

Application ACK is technical device-delivery acknowledgement only. It never means the human read or answered a message. PresetResponse carries its own PresetId + ResponseId context, so it can be accepted independently of any local waiting state. The contextual `ZACZEKAĆ?` action remains; sending Wait is simply another non-blocking PresetMessage and its later response is an ordinary self-contained PresetResponse.

The latest outgoing delivery state is serviced independently from foreground Communicator input. `WYSYLAM...` remains visible until the current logical operation resolves or is superseded; `DOSTARCZONO` / `NIE DOSTARCZONO` are non-modal results shown for about 2.5 s. Launcher caches the same narrow status so it can follow ordinary menu navigation without a notification queue. Delivery feedback never wakes DisplayOff and is suppressed behind Clock Glance, the Countdown alert, incoming Communicator content, SYGNAL, and RadioLab.

Working message screens do not repeat the `KOMUNIKATOR` identity header. Incoming presets prioritize the question and direct M5/BOCZNY hints. Response choice and received-response screens show smaller preset context above the dominant response; the optional wait decision preserves the same context-first presentation.

Communicator exposes one session-scoped radio-mode option through its messaging boundary. User-facing `STANDARD` maps to `radio::Mode::Normal`; `LR` maps to `radio::Mode::Lr`. The UI does not call `radio` directly. A successful mode change uses the existing radio mode switch, preserves delivery/dedupe/incoming state and the active RX profile/timing configuration, clears learned peer reachability/RSSI, and forces fresh PRESENCE discovery in the new mode. The selected Communicator mode is volatile for the OS boot and survives foreground exit, RadioLab handoff, and Communicator OFF -> ON within that boot. Full reboot resets the default to STANDARD.

The separate `SYGNAŁ` attention feature is a transient UI/audio overlay over the current foreground state. It reuses the existing RING delivery type but is not a preset message and does not enter the deterministic conversation state machine. Its short buzzer/animation sequence is advanced from the normal application update loop rather than a blocking delay or separate audio/animation framework.

RadioLab has a minimal lifecycle and temporary exclusive radio ownership. Entering RadioLab pauses messaging transport and starts RadioLab's continuous-RX radio session. Exiting RadioLab clears transient RadioLab state, stops that radio session, and resumes long-lived messaging transport.

### BatteryGuard

BatteryGuard is a small system policy component above `Board::power_status()`. It performs one safety sample approximately every 10 seconds, including while DisplayOff, independently from Launcher UI telemetry. `Board::power_status()` normalizes a non-positive AXP192 battery-voltage read to an invalid sample (`voltage_mv=-1`, `level_percent=-1`, `ChargeState::Unknown`) so a failed PMU/I2C read cannot look like deep discharge. Percentage is used only for advisory LOW/VERY_LOW thresholds; automatic shutdown uses confirmed valid battery voltage while not charging.

The guard owns only sampling cadence, threshold/hysteresis state, pending advisory severity, and critical confirmation. Recovery above the advisory re-arm threshold cancels that advisory if it is still pending; Charging clears pending advisory state and critical confirmation. `app_main` owns overlay priority, charging-driven removal of an already visible advisory, and the controlled shutdown presentation/cleanup. Battery policy is not embedded in Launcher, Communicator, RadioLab, or DisplayLifecycle.

### PowerDiag

PowerDiag is a volatile diagnostic measurement tool, not telemetry infrastructure. `PowerDiagSession` owns one RAM-only Inactive/Running session and has no rendering dependency. While Running, app_main feeds it a compact observation snapshot every normal main-loop iteration; the session accumulates 64-bit monotonic durations for total test time, LCD Active/Dimmed/DisplayOff, Communicator enabled time, Communicator foreground time, and RadioLab foreground time.

`PowerDiagApp` owns only the two diagnostic pages, START/NEW TEST confirmation, and local page/input state. Leaving the UI does not stop or reset the session. Re-entry reads the retained Snapshot. PowerDiag adds no task, event bus, generic logging/telemetry layer, NVS/flash persistence, or user-message counters.

Battery data reuses the exact `PowerStatus` already sampled by BatteryGuard. BatteryGuard exposes that same sampled value in its `UpdateResult`; app_main forwards it to `PowerDiagSession::record_battery_sample()` only when the safety sample occurs. PowerDiag never calls `Board::power_status()` and therefore adds no periodic PMU/I2C battery read. Invalid samples are ignored for baseline/current/minimum calculations.

Messaging exposes one read-only `current_rx_schedule()` accessor so app_main can observe the currently configured FG/BG interval and wake window without exposing or mutating the full transport configuration. PowerDiag observes RX profile/schedule, radio mode, peer known/reachable state, and latest valid RSSI only; it does not change messaging/radio policy.

PowerDiag is an ordinary foreground runtime for display purposes. DisplayOff suppresses its redraw work while the RAM session continues. Clock Glance, Countdown alert, BatteryGuard advisory, and accepted Communicator traffic retain their existing priority; restoring an ordinary temporary overlay redraws the previously open PowerDiag page. Incoming Communicator content may replace the PowerDiag foreground UI, while the diagnostic session continues.

## Architectural invariants

- RadioLab and Nikoś Communicator are sibling applications.
- Nikoś Communicator UI must not become a platform dependency.
- Applications must not call ESP-NOW APIs directly.
- Applications must not call `esp_wifi` APIs directly.
- The `radio` layer owns transport-facing Wi-Fi / ESP-NOW integration.
- `messaging::Service` owns Communicator delivery semantics above `radio`.
- RadioLab protocol and Communicator protocol remain separate while their requirements are materially different.
- ESP-NOW callbacks must perform minimal work and hand copied data to normal task context.
- UI state must not own background communication.
- Background messaging lifetime must remain independent of foreground Communicator visibility.
- Communicator background messaging is OFF after boot and must be explicitly enabled for the current OS session.
- Communicator enabled/disabled state is volatile and must not be persisted in NVS in this phase.
- Communicator is a latest-wins non-blocking pager, not chat history: only the newest received user message owns the UX.
- The transport queue is not a user-visible inbox; Communicator drains retained events and keeps the newest valid PresetMessage/PresetResponse.
- Human response state is independent from device-delivery state; there is no WaitingForResponse or suspended-conversation restoration model.
- Application ACK is the technical delivery acknowledgement; normal human conversation does not require a separate mandatory OK message.
- The contextual `ZACZEKAĆ?` follow-up remains because it carries conversational meaning rather than transport confirmation; its local-close path transmits nothing.
- Communicator physical UI shows one primary message/choice at a time and preserves secondary-short NEXT -> primary-short SELECT interaction on the two-button device.
- `SYGNAŁ` remains outside preset conversation semantics; it is a bounded transient attention overlay using existing RING delivery semantics.
- The launcher must not call ESP-NOW or `esp_wifi` APIs directly.
- The base launcher hierarchy is explicitly `Komunikator / Narzędzia / Rozrywka / Zegar / Ustawienia / Wyłącz`; category navigation stays shallow and fixed rather than becoming a generic menu framework.
- Every normal launcher submenu exposes a visible `Powrót` row; secondary-long may remain an optional shortcut.
- Launcher `WYŁĄCZ` means whole-device shutdown and remains distinct from Communicator `WYŁĄCZ` (service disable) and Communicator `POWRÓT` (foreground-panel exit).
- Whole-device shutdown orchestration belongs to `app_main`; M5-specific power-off belongs to `board`.
- Display lifecycle is independent from device and Communicator service lifecycle: `DisplayOff` means LCD/backlight only.
- The first user-button gesture that wakes `DisplayOff` is consumed through physical release; a fresh subsequent gesture is required for application action.
- Accepted user-visible Communicator content may wake the display, while transport-internal Presence/ACK/retry/TxResult/reachability activity does not.
- RTC validity relies on the hardware RTC read plus the RTC VL indication, not an NVS-configured flag.
- Clock Glance is a top-level transient UI driven only by `WakeReason::UserButton`; accepted Communicator content bypasses/cancels it immediately.
- Clock Glance timeout is ~4 s and returns directly to `DisplayOff` without entering `Dimmed`; normal Active/Dimmed/DisplayOff timing resumes after glance dismissal.
- Countdown timing is monotonic and independent from RTC; changing or losing wall-clock time cannot change a running Timer.
- Running or paused Countdown state survives foreground UI changes and DisplayOff but not whole-device shutdown.
- Countdown expiration is a one-shot pending event until user acknowledgment; finite sound completion does not acknowledge it.
- Accepted Communicator traffic has priority over the Timer alert, while the pending Timer event survives that preemption.
- An unacknowledged Timer expiration bypasses/cancels Clock Glance; a merely running or paused Timer does not.
- RadioLab may temporarily take exclusive radio ownership only through the explicit messaging pause/resume handoff.
- RadioLab pauses/resumes messaging only when Communicator messaging was active before the handoff; RadioLab exit must never start an OFF messaging session.
- ESP-NOW MAC send success is not application-level delivery.
- Communicator delivery confirmation requires a matching application ACK; attempt-budget or deadline exhaustion produces an explicit Failed logical outcome.
- Communicator Presence is discovery-oriented, not a continuous liveness heartbeat. Once the one peer is known, normal idle operation does not require periodic Presence transmission; peer identity and recent-RX status are distinct, and bounded delivery may target a known peer after recent-RX status becomes stale.
- A valid broadcast Presence learns/confirms the peer and schedules one serialized unicast Presence reply. A received unicast Presence learns/confirms the peer but never schedules another Presence reply.
- Communicator retry interval/jitter, attempt limit, and logical timeout are experimental configuration; a known peer remains eligible for bounded delivery even when recent-RX status is stale, and that stale time continues consuming the logical delivery deadline.
- Deliberate RadioLab transport ownership pause is different from peer unreachability: it suspends logical delivery/retry timing, and resume preserves the same MessageId, attempts, and remaining timing budget.
- Current metrics count ESP-NOW send submissions/requests rather than true PHY-level Wi-Fi transmissions; MAC TxResult is recorded separately and never represents application delivery.
- Messaging peer-unicast submissions are serialized across logical payloads, application ACKs, and one-shot unicast discovery Presence replies; broadcast discovery Presence remains separate traffic while no peer is known.
- A missing messaging-unicast TxResult must recover through a bounded attribution barrier before any newer peer unicast can be attributed.
- If that attribution-barrier radio restart cannot be re-established, messaging fails closed: any still-active logical delivery completes as Failed, stale unicast/ACK work is discarded, peer reachability is invalidated, and new logical sends remain rejected until the normal Communicator service lifecycle performs stop() followed by a fresh begin().
- Duplicate logical messages may be ACKed again but must not create duplicate user notification events.
- A new incoming logical message is application-ACKed/deduped only after the small messaging queue has retained it; foreground consumers consume it only after accepting it.
- RSSI is receiver-side radio metadata and must not be treated as physical distance.
- Communicator STANDARD/LR selection is session-scoped radio configuration owned by `messaging::Service`; it must not alter foreground/background RX schedules, presence cadence, retry timing, ACK/dedupe semantics, or logical MessageIds.
- Settings signal-sound and visual-theme selections are boot-scoped volatile state owned by composition; neither is persisted in NVS.
- Settings preview and received Communicator `SYGNAŁ` must use the same `signal_sound::Player` and fixed pattern definitions.
- Applications request semantic display roles; theme-specific RGB565 values remain centralized in `ui_theme` and are resolved by `board`.
- Theme accent is distinct from semantic status/attention/danger roles; Noir deliberately resolves those semantic roles to monochrome white/gray presentation.
- Experimental RX and recent-RX timing values are configuration, not send-permission invariants. The current foreground profile uses an approximately 7 s recent-RX timeout and the background profile approximately 20 s; after that age the UI may show a neutral known-peer state, but peer identity is retained and bounded delivery remains allowed.
- Persistent schemas and wire protocols must be versioned once introduced.
