# Entertainment Guard v2 — accumulated 20 / idle 15 / cooldown 60

## Policy and state ownership

The one Guard in `main/app_main.cpp` is the owner of the entertainment
allowance. No game, Facts or VW Garage owns its own global usage counter.

| Phase | Admission | Accounting | Exit |
| --- | --- | --- | --- |
| Available | Allowed after persistence cleanup | None | Enter: reserve first checkpoint and commit before launching |
| Window | Allowed with the existing remaining shared balance | Visible guarded runtime only, including in-app pause/menus | 15 min continuously not visible resets unfinished session; 20 min charged starts Cooldown |
| Cooldown | Always denied | None | 60 min monotonic while powered, or bounded trusted UTC elapsed after reboot; NVS clear must succeed before admission |

Switching games, returning to Entertainment/Games menus, checking another tool,
Communicator takeover, DisplayOff and charging lock all leave the same shared
balance. The 15-minute timer measures *continuous absence of active, visible
entertainment*, not absence of button presses. A warning overlay suspends
accounting while visible. At 18 minutes accumulated, the warning appears once
per unfinished session. At 20 minutes, the existing central preemption shuts
the guarded app even if an incoming Communicator interaction owns the screen.

Visible means `is_guarded_entertainment(state)` **and** screen not DisplayOff,
not charging-locked, not Clock Glance, not battery/timer advisory, and not the
warning overlay. The main loop evaluates this centrally and calls
`Guard::tick()`, using wrap-safe elapsed uptime. Runtime transitions are
bounded by a single iteration (nominally 20 ms).

## ENT3 storage, UTC and migration

Existing NVS namespace/key `nikos/ent_cycle` are unchanged.
Records are differentiated by marker and exact size:

- ENT1 (12 bytes): local civil minutes, converted by the existing
  legacy local-minute migration helper. Invalid local civil stamps fail closed.
- ENT2 (12 bytes): UTC civil minutes. The original 20+60 wall-time window is
  migrated conservatively: the whole elapsed old window is charged as active
  use; an existing cooldown retains its original deadline; expired records
  are cleared. Active older records are never treated as brand new sessions.
- ENT3 (24 bytes): `marker, used_ms, anchor_utc_s, flags, reserved`.
  Flags mark RTC trust, prior warning, paused state, and mandatory cooldown.
  `anchor_utc_s` is either the last visible/pause transition or the start
  of the cooldown. Malformed record, bad marker/length/flags, NVS read failure,
  or untrusted/rolled-back RTC with a live record => full 60-minute
  fail-closed fallback from this boot.

During continuous foreground use, a committed checkpoint **reserves up to
60 seconds ahead**. The local runtime only charges actual visible elapsed
time. The next reservation is committed when reached; returning to
non-entertainment writes the precise actual used time. This prevents
ordinary sudden power loss from rolling back the meter more than a reserved
interval: any unspent reservation is a deliberate conservative loss after
crash/reboot. Significant changes (enter, exit, warning, cooldown, completion)
are committed immediately and `nvs_set_blob` / `nvs_erase_key` and
`nvs_commit` outcomes are checked. No per-frame flash writes or background
worker are introduced.

**Power loss while a guarded app was visible:** ENT3 does *not* use the
checkpoint UTC anchor as permission to reset after 15 minutes. It restores
the pessimistic stored used budget and requires 15 new minutes of inactivity.
An already paused ENT3 record may count trusted UTC inactivity across power
off, with the original 15-minute threshold. Untrusted time on reboot starts
a new mandatory fallback cooldown; it never grants unused allowance.
A trusted cooldown uses the stored UTC deadline. An untrusted or rollback
cooldown restarts a *full* 60-minute monotonic fallback and cannot be bypassed
by the 15-minute inactivity reset.

**Failure semantics:** Failed initial admission commit denies launch. Any
checkpoint/transition failure during a running session locks it for a
60-minute fallback in RAM and preempts the app. A failed clear remains pending
and prevents admission; it is retried on an explicit future admission, not
each frame. If a device has a physically failing/unwritable flash medium,
software cannot durably record events that never reached flash; the last
successful conservative reservation remains the only recoverable proof.
The device must not be treated as tamper-resistant against deliberate NVS
erasure or RTC manipulation.

## Host and device verification

Deterministic C++ regression exercises 20-minute exhaustion, 15-minute
unfinished reset, short breaks, accumulated warning, mode visibility,
RTC rollback/unavailability, recovery of active/paused/cooldown records,
NVS save/clear error behavior, and uint32 uptime wrap. Legacy UTC/ENT1
migration regression exercises original 20+60 expiry.

On hardware, verify both Sticks independently: repeated app switches,
POWER off/on, incoming Communicator takeover, charging lock and override,
low battery reserve, 18-minute warning/20-minute hard preemption,
15-minute unfinished reset with and without reboot, and full cooldown with
trusted and untrusted RTC. Confirm no unexpected launcher rendering behind
high-priority system overlays; inspect UART for NVS warnings.

No flash, no merge; a separate hardware audit must precede release.
