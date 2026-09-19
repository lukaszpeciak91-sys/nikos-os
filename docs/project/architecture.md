# Architecture

## High-level direction

Nikoś OS is a small modular embedded platform. It is not firmware dedicated only to the Nikoś Communicator.

The conceptual model is:

`BOOT -> PLATFORM / CORE -> lightweight launcher -> applications`

Applications may initially be compiled into a single firmware image. No dynamic APK-style or plugin system is required.

The current runtime starts with a lightweight launcher. The launcher has three fixed entries: RadioLab, Minutnik, and Rozrywka. Only RadioLab is active; the other two are placeholders. This is a static skeleton, not an application registry or plugin framework.

## Initial ownership boundaries

### board

Owns M5-specific hardware integration:

- LCD
- buttons
- buzzer
- AXP192 / PMU
- battery information
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
- a small volatile queue of incoming logical message notifications
- delivery receipts for matching application ACKs
- foreground/background experimental RX profile selection

The service has no dedicated FreeRTOS task. It is advanced from the normal main loop.

Messaging state is independent of foreground UI. While RadioLab owns the radio for its field-test session, messaging transport is paused but messaging state remains alive. When RadioLab exits, messaging transport resumes.

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

Initial and future applications include:

- RadioLab
- future Nikoś Communicator UI
- future diagnostic, Wi-Fi, BLE, IR, and hardware tools

RadioLab v0.1 uses equal peers running the same firmware. It does not assign permanent BASE/MOBILE roles.

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
- Background messaging must remain independent of the foreground screen/application.
- The launcher must not call ESP-NOW or `esp_wifi` APIs directly.
- RadioLab may temporarily take exclusive radio ownership only through the explicit messaging pause/resume handoff.
- ESP-NOW MAC send success is not application-level delivery.
- Communicator delivery confirmation requires a matching application ACK.
- Duplicate logical messages may be ACKed again but must not create duplicate user notification events.
- RSSI is receiver-side radio metadata and must not be treated as physical distance.
- Experimental RX timing values are configuration, not platform invariants.
- Persistent schemas and wire protocols must be versioned once introduced.
