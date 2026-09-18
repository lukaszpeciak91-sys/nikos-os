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

RadioLab proves and diagnoses platform capabilities. The future Nikoś Communicator uses the same platform services.

**Rationale:** The communicator must not define the platform or become a dependency of platform services.

## D-004 — Automatic bootstrap discovery without pairing UX

**Status:** Accepted

RadioLab v0.1 devices periodically broadcast a versioned DISCOVERY packet on a fixed channel. A compatible device may learn one peer MAC from that packet and register it for unicast ESP-NOW traffic.

There is no pairing screen, device list, account/name system, or persistent peer database. Rediscovery after reboot is expected.

**Rationale:** The first field test uses two equal devices and should not require manual MAC entry, while avoiding a general-purpose pairing framework.

## D-005 — Application ACK defines end-to-end delivery

**Status:** Accepted

ESP-NOW send callback success is only a MAC-level result. An application ACK confirms peer-side application processing. RadioLab uses the application ACK as its end-to-end delivery signal.

**Rationale:** MAC-level send completion and peer-side application processing are different guarantees and must be reported separately.

## D-006 — RSSI remains receiver-side metadata

**Status:** Accepted

For a RadioLab PING exchange:

- the device receiving PING measures that packet's RX RSSI;
- the ACK may report that measured RSSI to the initiating peer;
- the device receiving ACK independently measures the ACK packet's RX RSSI.

RadioLab may display both values but must not convert RSSI into metres or another physical distance estimate.

**Rationale:** RSSI is receiver-side signal metadata affected by environment, orientation, obstruction, antenna characteristics, and other variables that make direct distance inference unreliable.

## D-007 — Benchmark radio mode is explicitly selected

**Status:** Accepted

Each RadioLab benchmark run uses an explicitly selected radio mode: `NORMAL` or `LR`. RadioLab does not automatically switch between NORMAL and LR during a benchmark run.

NORMAL uses the standard ESP32 802.11 b/g/n protocol bitmap. LR uses the Espressif LR-only protocol bitmap.

**Rationale:** Keeping the radio mode fixed preserves measurement validity and makes benchmark results comparable.

## D-008 — RadioLab v0.1 devices are equal peers

**Status:** Accepted

Both physical test devices run the same firmware and expose the same discovery, PING, ACK, HELLO, LIVE, and mode-selection behavior.

**Rationale:** Home/carried placement is a test circumstance, not a permanent radio role. Either unit must be usable in either position.
