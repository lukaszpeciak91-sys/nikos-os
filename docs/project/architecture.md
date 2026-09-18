# Architecture

## High-level direction

Nikoś OS is a small modular embedded platform. It is not firmware dedicated only to the Nikoś Communicator.

The conceptual model is:

`BOOT -> PLATFORM / CORE -> lightweight launcher -> applications`

Applications may initially be compiled into a single firmware image. No dynamic APK-style or plugin system is required.

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
- fixed Wi-Fi channel
- NORMAL / Espressif LR mode
- raw broadcast and unicast TX/RX
- RX radio metadata
- MAC-level send result

ESP-NOW callbacks perform only bounded copying into a queue. Application logic executes later in normal task context.

### protocol

Owns the application-independent on-air representation:

- versioned and identifiable on-air data format
- message type
- identifiers and sequence information
- explicit encode/decode responsibilities

### storage

Owns versioned persistent configuration when persistence is required.

No storage layer is required by the current RadioLab milestone.

### power

Future owner of:

- sleep policy
- display and backlight power policy
- radio power policy

Do not implement this layer yet.

### applications

Initial and future applications include:

- RadioLab
- future Nikoś Communicator
- future diagnostic, Wi-Fi, BLE, IR, and hardware tools

RadioLab v0.1 uses equal peers running the same firmware. It does not assign permanent BASE/MOBILE roles.

## Architectural invariants

- RadioLab and Nikoś Communicator are sibling applications.
- Nikoś Communicator must not become a platform dependency.
- Applications must not call ESP-NOW APIs directly.
- Applications must not call `esp_wifi` APIs directly.
- The `radio` layer owns transport-facing Wi-Fi / ESP-NOW integration.
- ESP-NOW callbacks must perform minimal work and hand copied data to normal task context. Application logic must not execute directly inside Wi-Fi callbacks.
- UI state must not become the owner of background communication.
- RadioLab field placement is not a persistent device role; either peer may remain at home or be carried.
- ESP-NOW send callback success is not application-level delivery.
- RSSI is receiver-side radio metadata and must not be treated as physical distance.
- Persistent schemas and wire protocols must be versioned once introduced.
