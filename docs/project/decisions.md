# Project Decisions

This file records concise accepted project decisions. Add new records only when a decision becomes part of the authoritative project state.

## D-001 — Use native ESP-IDF

**Status:** Accepted

Native ESP-IDF is the primary framework.

**Rationale:** It provides direct control over ESP-NOW, Wi-Fi configuration, RX metadata, Espressif LR mode, power management, NVS, and FreeRTOS.

**Current direction:** Use a stable, pinned ESP-IDF 5.5.x release.

## D-002 — Use M5Unified/M5GFX for M5 hardware support

**Status:** Accepted

Use M5Unified/M5GFX as ESP-IDF components for initial M5 hardware support.

**Rationale:** Existing maintained hardware support should be preferred over custom LCD or AXP192 drivers unless a real limitation later justifies replacement.

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

ESP-NOW send callback success must not be interpreted as confirmed application delivery.

**Rationale:** MAC-level send completion and application-level receipt are different guarantees. End-to-end delivery requires an application-level acknowledgement.

## D-006 — No physical distance estimation from RSSI

**Status:** Accepted

RadioLab may display RSSI but must not convert RSSI into metres or another physical distance estimate.

**Rationale:** RSSI is receiver-side radio metadata affected by environment, orientation, obstruction, antenna characteristics, and other variables that make direct distance inference unreliable.

## D-007 — Minimal two-button UI model

**Status:** Accepted

The current proposed global navigation model is:

- Button B short -> NEXT
- Button A short -> SELECT / ACTION
- Button B long -> BACK
- Button A long -> initially unused / future application-specific action

The power button is reserved for power and sleep semantics rather than ordinary navigation.

**Rationale:** The model fits the two user buttons exposed by the M5StickC Plus SE while keeping navigation simple and leaving one long-press action available for future application needs.

**Verification note:** Actual hardware button mapping must still be verified on physical units before implementation depends on it.
