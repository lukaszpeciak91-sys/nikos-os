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

Owns radio hardware and transport-facing behavior:

- Wi-Fi / ESP-NOW initialization
- peer configuration
- Wi-Fi channel
- NORMAL / Espressif LR mode
- raw TX/RX
- RX radio metadata
- MAC-level send result

### protocol

Owns the application-independent on-air representation:

- versioned on-air data format
- message type
- identifiers and sequence information
- explicit encode/decode responsibilities

### storage

Owns versioned persistent configuration when persistence is required.

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

## Architectural invariants

- RadioLab and Nikoś Communicator are sibling applications.
- Nikoś Communicator must not become a platform dependency.
- Applications must not directly own or configure ESP-NOW or Wi-Fi hardware.
- ESP-NOW callbacks must perform minimal work and hand data to normal task context. Application logic must not execute directly inside Wi-Fi callbacks.
- UI state must not become the owner of background communication.
- Device profile and radio role are separate concepts: `DEV / NIKOS` != `BASE / MOBILE`.
- ESP-NOW send callback success is not application-level delivery.
- RSSI is receiver-side radio metadata and must not be treated as physical distance.
- Persistent schemas and wire protocols must be versioned once introduced.
