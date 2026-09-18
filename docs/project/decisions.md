# Project Decisions

This file records concise accepted project decisions. Add new records only when a decision becomes part of the authoritative project state.

## D-001 — Use native ESP-IDF

**Status:** Accepted

Native ESP-IDF is the primary framework.

**Rationale:** It provides direct control over ESP-NOW, Wi-Fi configuration, RX metadata, Espressif LR mode, power management, NVS, and FreeRTOS.

**Current implementation:** ESP-IDF 5.5.5 is pinned exactly for the initial hardware sanity milestone.

## D-002 — Use M5Unified/M5GFX for M5 hardware support

**Status:** Accepted

Use M5Unified/M5GFX as ESP-IDF components for initial M5 hardware support.

**Rationale:** Existing maintained hardware support should be preferred over custom LCD or AXP192 drivers unless a real limitation later justifies replacement.

**Current implementation:** M5Unified 0.2.22 and M5GFX 0.2.29 are pinned through the ESP-IDF Component Manager.

## D-003 — RadioLab and Communicator are sibling applications

**Status:** Accepted

RadioLab proves and diagnoses platform capabilities. The future Nikoś Communicator uses the same platform services.

**Rationale:** The communicator must not define the platform or become a dependency of platform services.

## D-004 — No pairing/discovery in RadioLab v0.1

**Status:** Accepted

RadioLab v0.1 assumes two known devices. Peer configuration may initially be explicit or static.

**Rationale:** Pairing and discovery are postponed until a real requirement justifies the additional behavior and UI.

## D-005 — Application ACK defines end-to-end delivery

**Status:** Accepted

ESP-NOW send callback success is only a MAC-level result. An application ACK confirms peer-side application processing. RadioLab uses the application ACK as its end-to-end delivery signal.

**Rationale:** MAC-level send completion and peer-side application processing are different guarantees and must be reported separately.

## D-006 — No physical distance estimation from RSSI

**Status:** Accepted

RadioLab may display RSSI but must not convert RSSI into metres or another physical distance estimate.

For RadioLab measurements:

- BASE measures RSSI from packets received from MOBILE.
- MOBILE measures RSSI from ACK packets received from BASE.

RSSI remains receiver-side signal metadata and must not be interpreted as physical distance.

**Rationale:** RSSI is affected by environment, orientation, obstruction, antenna characteristics, and other variables that make direct distance inference unreliable.

## D-007 — Benchmark radio mode is explicitly selected

**Status:** Accepted

Each RadioLab benchmark run uses an explicitly selected radio mode: `NORMAL` or `LR`. RadioLab does not automatically switch between NORMAL and LR during a benchmark run.

**Rationale:** Keeping the radio mode fixed preserves measurement validity and makes benchmark results comparable.
