# Architecture

## High-level direction

Nikoś OS is a small modular embedded platform. It is not firmware dedicated only to the Nikoś Communicator.

The conceptual model is:

`BOOT -> PLATFORM / CORE -> lightweight launcher -> applications`

Applications may initially be compiled into a single firmware image. No dynamic APK-style or plugin system is required.

The current runtime starts with a lightweight launcher. Its frozen base top-level hierarchy is: `Komunikator`, `Narzędzia`, `Rozrywka`, `Zegar`, `Ustawienia`, and final whole-device `Wyłącz`. The launcher keeps a small local four-row viewport so the six fixed entries do not overlap the header/footer.

The hierarchy is deliberately shallow and explicit:
- `Komunikator` owns the existing communication lifecycle entry/enable UI.
- `Narzędzia` contains `RadioLab` plus visible `Powrót`; RadioLab remains an application sibling of Communicator even though it is launched through the tools category.
- `Rozrywka`, `Zegar`, and `Ustawienia` currently contain only visible `Powrót` rows and reserve semantic space for future entertainment, time-related tools, and configuration respectively.
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

Owns the first versioned Communicator wire format.

Current technical message types are:

- PRESENCE
- PRESET_MESSAGE
- PRESET_RESPONSE
- ACK
- RING

Logical message IDs and ACK reference IDs are part of the Communicator wire format. The protocol carries compact preset/response IDs rather than user-visible strings.

This component is intentionally separate from RadioLab protocol. It does not establish a generic messaging protocol framework and does not prevent future message types such as free text.

### messaging

`messaging::Service` is the long-lived Communicator transport/delivery service above `radio`.

It currently owns:

- one known peer slot
- one-peer discovery Presence state, distinct peer identity, and recent-RX/reachability status
- latest peer RX RSSI
- stable logical message IDs across retries
- one outstanding outgoing logical message
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
- a small volatile queue of incoming logical message notifications, exposed through explicit peek/consume so UI rejection cannot destructively remove an event
- delivery completion receipts for matching application ACKs or explicit failure
- foreground/background experimental RX profile selection, including profile-aware reachability timeout

The service has no dedicated FreeRTOS task. It is advanced from the normal main loop. Communicator retry interval/jitter, maximum send attempts, and logical delivery timeout are experimental configuration for hardware tuning rather than permanent product policy. Application ACK remains the only authoritative Delivered condition. ESP-NOW TxResult is consumed only as a pacing/measurement signal: MAC success lengthens the ACK wait, MAC failure schedules the existing bounded retry, and a missing callback has bounded recovery. Presence is discovery-oriented: while no peer is known, broadcast discovery uses the experimental 2000 ms + 0…250 ms cadence; once the peer is known, normal idle operation stops periodic Presence TX. A received broadcast Presence schedules one serialized unicast Presence reply, while a received unicast Presence never triggers another reply.

Messaging service lifetime is independent of foreground Communicator UI. Background Communicator messaging starts OFF after boot and is enabled explicitly for the current OS session only. This enabled/disabled state is volatile and is not persisted in NVS.

While enabled, leaving the foreground Communicator UI does not stop messaging; it restores the background RX profile. Explicitly disabling Communicator stops the messaging transport and clears volatile messaging/session state.

While RadioLab owns the radio for its field-test session, messaging transport is paused only if Communicator messaging was active before the handoff. RadioLab exit resumes messaging only in that case; it must not start an OFF Communicator session.

Receiver dedupe state is part of that long-lived in-memory service state, so it survives foreground application changes and the RadioLab pause/resume handoff. It is intentionally volatile across a full device reboot. In this first infrastructure version, if a sender is still retrying an outstanding logical message when the receiver reboots, that message may be surfaced again after the receiver restarts.

### settings

`settings::State` owns the current boot-scoped user preference state. It now contains the two real runtime preferences:

- Communicator `SYGNAŁ` sound: Gentle / `Łagodny` (default), Classic / `Klasyczny`, or Pager;
- visual theme: Nikoś (default), Bursztyn, or Grafit.

The state is created by composition in `app_main`, is shared with the launcher Settings UI and the narrow consumers that need each typed value, and is intentionally volatile across reboot. No NVS or persistent settings schema is introduced yet.

### ui_theme

`ui_theme` owns the three fixed compile-time visual palettes:

- Nikoś — the approved near-black navy reference theme;
- Bursztyn — a dark warm retro-electronic palette;
- Grafit — a neutral high-readability graphite palette.

Applications request semantic display roles rather than RGB565 values. Theme-varying roles are `Background`, `Surface`, `PrimaryText`, `SecondaryText`, and `Accent`. Product-semantic roles such as `StatusActive`, `StatusInactive`, `Attention`, and `Danger` remain fixed across themes so status, SYGNAŁ attention, and error/destructive meaning do not drift with the selected palette.

The active palette is held by `board` and selected from `settings::State::theme`. No generic styling engine, per-screen palette, or runtime RGB editor is introduced.

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

Future owner of product-level:

- sleep policy
- display and backlight power policy
- radio power policy

Whole-device shutdown is intentionally narrower than a power-policy framework: the launcher emits only a shutdown request, `app_main` performs orderly runtime cleanup, and the board layer owns the M5-specific `M5.Power.powerOff()` call.

The current messaging RX schedules are experimental transport configuration, not permanent power architecture.

### launcher

Owns only the current fixed launcher navigation and its explicit shallow category screens.

It does not own radio lifecycle internals, application registries, persistence, profiles, plugin loading, or hardware power control. The launcher may request whole-device shutdown only after explicit confirmation. RadioLab is launched from `Narzędzia`; after RadioLab exits, the launcher returns to `Narzędzia` rather than MAIN.

### applications

Current and future applications include:

- Communicator v0.1
- RadioLab
- future diagnostic, Wi-Fi, BLE, IR, and hardware tools

RadioLab v0.1 uses equal peers running the same firmware. It does not assign permanent BASE/MOBILE roles.

Communicator is a foreground UI over the session-scoped `messaging::Service`. Enabling Communicator starts the service for the current OS session. Entering the foreground panel selects the experimental foreground messaging RX profile; exiting the panel—either through the selectable `POWRÓT` item or the secondary-long shortcut—restores the background profile without disabling the service. Incoming Communicator traffic may surface the Communicator UI from the launcher without moving delivery/retry logic into UI state. To prevent FIFO head-of-line blocking during one active exchange, Communicator may hold exactly one temporarily incompatible incoming logical event locally while later service-queue traffic is inspected; this is current-exchange state, not a general inbox/router.

Communicator exposes one session-scoped radio-mode option through its messaging boundary. User-facing `STANDARD` maps to `radio::Mode::Normal`; `LR` maps to `radio::Mode::Lr`. The UI does not call `radio` directly. A successful mode change uses the existing radio mode switch, preserves delivery/dedupe/incoming state and the active RX profile/timing configuration, clears learned peer reachability/RSSI, and forces fresh PRESENCE discovery in the new mode. The selected Communicator mode is volatile for the OS boot and survives foreground exit, RadioLab handoff, and Communicator OFF -> ON within that boot. Full reboot resets the default to STANDARD.

The separate `SYGNAŁ` attention feature is a transient UI/audio overlay over the current foreground state. It reuses the existing RING delivery type but is not a preset message and does not enter the deterministic conversation state machine. Its short buzzer/animation sequence is advanced from the normal application update loop rather than a blocking delay or separate audio/animation framework.

RadioLab has a minimal lifecycle and temporary exclusive radio ownership. Entering RadioLab pauses messaging transport and starts RadioLab's continuous-RX radio session. Exiting RadioLab clears transient RadioLab state, stops that radio session, and resumes long-lived messaging transport.

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
- Communicator conversation state is small, volatile, and limited to the current deterministic exchange; it is not chat history.
- Communicator may retain at most one deferred incoming event to avoid head-of-line blocking; it must not overwrite that slot or expand it into a general inbox/reordering layer.
- True simultaneous conversational initiation uses deterministic MAC ordering: the lower self MAC temporarily yields and may suspend exactly one WaitingForResponse context until the peer's short exchange completes; this is collision handling, not multi-conversation scheduling.
- Human-visible conversation `OK` remains distinct from transport/application ACK.
- `SYGNAŁ` remains outside preset conversation semantics; it is a bounded transient attention overlay using existing RING delivery semantics.
- The launcher must not call ESP-NOW or `esp_wifi` APIs directly.
- The base launcher hierarchy is explicitly `Komunikator / Narzędzia / Rozrywka / Zegar / Ustawienia / Wyłącz`; category navigation stays shallow and fixed rather than becoming a generic menu framework.
- Every normal launcher submenu exposes a visible `Powrót` row; secondary-long may remain an optional shortcut.
- Launcher `WYŁĄCZ` means whole-device shutdown and remains distinct from Communicator `WYŁĄCZ` (service disable) and Communicator `POWRÓT` (foreground-panel exit).
- Whole-device shutdown orchestration belongs to `app_main`; M5-specific power-off belongs to `board`.
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
- Theme accent is distinct from fixed semantic status/attention/danger colors.
- Experimental RX and recent-RX timing values are configuration, not send-permission invariants. The current foreground profile uses an approximately 7 s recent-RX timeout and the background profile approximately 20 s; after that age the UI may show a neutral known-peer state, but peer identity is retained and bounded delivery remains allowed.
- Persistent schemas and wire protocols must be versioned once introduced.
