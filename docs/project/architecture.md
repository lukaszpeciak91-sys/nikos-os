# Architecture

## High-level direction

Nikoś OS is a small modular embedded platform. It is not firmware dedicated only to the Nikoś Communicator.

The conceptual model is:

`BOOT -> PLATFORM / CORE -> lightweight launcher -> applications`

Applications may initially be compiled into a single firmware image. No dynamic APK-style or plugin system is required.

The current runtime starts with a lightweight launcher. The launcher has four fixed entries: Communicator, RadioLab, Minutnik, and Rozrywka. Communicator and RadioLab are real applications; Minutnik and Rozrywka remain placeholders. This is still a static shape, not an application registry or plugin framework.

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
- presence/reachability state
- latest peer RX RSSI
- stable logical message IDs across retries
- one outstanding outgoing logical message
- retry-until-application-ACK behavior, with retransmission suspended while the known peer is stale/unreachable
- receiver-side in-memory dedupe
- duplicate ACK behavior without duplicate notification
- bounded configurable presence jitter to avoid deterministic aliasing with duty-cycled RX schedules
- a small volatile queue of incoming logical message notifications, exposed through explicit peek/consume so UI rejection cannot destructively remove an event
- delivery receipts for matching application ACKs
- foreground/background experimental RX profile selection, including profile-aware reachability timeout

The service has no dedicated FreeRTOS task. It is advanced from the normal main loop.

Messaging service lifetime is independent of foreground Communicator UI. Background Communicator messaging starts OFF after boot and is enabled explicitly for the current OS session only. This enabled/disabled state is volatile and is not persisted in NVS.

While enabled, leaving the foreground Communicator UI does not stop messaging; it restores the background RX profile. Explicitly disabling Communicator stops the messaging transport and clears volatile messaging/session state.

While RadioLab owns the radio for its field-test session, messaging transport is paused only if Communicator messaging was active before the handoff. RadioLab exit resumes messaging only in that case; it must not start an OFF Communicator session.

Receiver dedupe state is part of that long-lived in-memory service state, so it survives foreground application changes and the RadioLab pause/resume handoff. It is intentionally volatile across a full device reboot. In this first infrastructure version, if a sender is still retrying an outstanding logical message when the receiver reboots, that message may be surfaced again after the receiver restarts.

### storage

Owns versioned persistent configuration when persistence is required.

No persistent contact database or chat history is introduced by the first Communicator infrastructure phase.

### power

Future owner of product-level:

- sleep policy
- display and backlight power policy
- radio power policy

The current messaging RX schedules are experimental transport configuration, not permanent power architecture.

### launcher

Owns only the current top-level selection UI and fixed launcher navigation.

It does not own radio lifecycle internals, application registries, persistence, profiles, or plugin loading.

### applications

Current and future applications include:

- Communicator v0.1
- RadioLab
- future diagnostic, Wi-Fi, BLE, IR, and hardware tools

RadioLab v0.1 uses equal peers running the same firmware. It does not assign permanent BASE/MOBILE roles.

Communicator is a foreground UI over the session-scoped `messaging::Service`. Enabling Communicator starts the service for the current OS session. Entering the foreground panel selects the experimental foreground messaging RX profile; exiting the panel restores the background profile without disabling the service. Incoming Communicator traffic may surface the Communicator UI from the launcher without moving delivery/retry logic into UI state. To prevent FIFO head-of-line blocking during one active exchange, Communicator may hold exactly one temporarily incompatible incoming logical event locally while later service-queue traffic is inspected; this is current-exchange state, not a general inbox/router.

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
- RadioLab may temporarily take exclusive radio ownership only through the explicit messaging pause/resume handoff.
- RadioLab pauses/resumes messaging only when Communicator messaging was active before the handoff; RadioLab exit must never start an OFF messaging session.
- ESP-NOW MAC send success is not application-level delivery.
- Communicator delivery confirmation requires a matching application ACK.
- Duplicate logical messages may be ACKed again but must not create duplicate user notification events.
- A new incoming logical message is application-ACKed/deduped only after the small messaging queue has retained it; foreground consumers consume it only after accepting it.
- RSSI is receiver-side radio metadata and must not be treated as physical distance.
- Experimental RX and reachability timing values are configuration, not platform invariants. The current foreground profile uses an approximately 7 s reachability timeout, while the background 3000/500 ms RX profile uses a more conservative approximately 20 s timeout to tolerate legitimately missed PRESENCE packets.
- Persistent schemas and wire protocols must be versioned once introduced.
