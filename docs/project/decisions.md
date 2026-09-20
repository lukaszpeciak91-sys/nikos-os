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

For the first implementation it supports one known peer, presence/reachability, latest peer RSSI, one outstanding outgoing logical message, retry until matching application ACK, and receiver dedupe. Retransmission is suspended while the known peer is stale/unreachable and resumes with the same logical message ID after valid peer traffic restores reachability. Duplicate copies are ACKed again but do not produce duplicate notification events.

Presence cadence includes small bounded configurable jitter so deterministic schedules do not repeatedly alias with duty-cycled receive windows.

The service is advanced by the main loop and does not own a separate FreeRTOS task. Its dedupe state is in memory: it survives foreground application changes and RadioLab messaging pause/resume, but not a full device reboot. A sender still retrying across a receiver reboot may therefore cause that logical message to be surfaced again in this first infrastructure version.

**Rationale:** Background communication needs persistent delivery state without tying it to a particular screen or introducing a generic messaging framework.

## D-011 — RadioLab temporarily owns radio during its session

**Status:** Accepted

Long-lived messaging normally owns the active radio transport. Entering RadioLab pauses messaging transport; RadioLab then starts its own continuous-RX session. Exiting RadioLab stops that session and resumes messaging transport.

**Rationale:** This preserves the existing field-test behavior while allowing messaging state to remain alive across foreground application changes.

## D-012 — Messaging RX duty profiles are experimental configuration

**Status:** Accepted

The first messaging foundation provides configurable foreground/background ESP-NOW RX schedules with profile-aware reachability timeouts.

Current experimental starting values are:
- foreground: approximately 1000/500 ms RX schedule with approximately 7000 ms reachability timeout;
- background: approximately 3000/500 ms RX schedule with approximately 20000 ms reachability timeout.

These values are not permanent product or platform policy.

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
