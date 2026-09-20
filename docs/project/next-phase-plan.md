# Next Phase Plan

> **Temporary implementation-phase checklist**
>
> This file tracks the currently agreed near-term Nikoś OS product and technical direction. It is operational planning, not permanent architecture.
>
> Check items off as they are actually implemented and verified. When this phase is substantially complete, this file may be moved to an archive/history location. Permanent architectural decisions must continue to live in the authoritative project documentation.

## Communicator technical foundation

The first infrastructure phase intentionally supports one known peer and keeps messaging alive independently of foreground UI. A peer-selection screen is not a prerequisite for the first Communicator UI.

- [x] Add a separate versioned `communicator_protocol`.
- [x] Add a long-lived `messaging::Service` above `radio`.
- [x] Support one known peer slot.
- [x] Track presence/reachability and latest peer RX RSSI.
- [x] Add small bounded configurable presence jitter to avoid deterministic RX-window aliasing.
- [x] Use stable logical message IDs across retries.
- [x] Support PRESET_MESSAGE, PRESET_RESPONSE, ACK, and RING wire types.
- [x] Retry one outgoing logical message until its matching application ACK.
- [x] Suspend retransmission while the known peer is stale/unreachable and resume the same logical message ID after recovery.
- [x] Dedupe received logical messages while ACKing duplicate copies again.
- [x] Keep incoming logical events retained until Communicator accepts them; use non-destructive peek plus explicit consume.
- [x] Allow exactly one temporarily incompatible incoming event to be deferred locally so it cannot head-of-line block a later event required by the active exchange.
- [x] Resolve true simultaneous conversational initiation deterministically by MAC ordering, with exactly one suspended WaitingForResponse context on the yielding side.
- [x] Keep dedupe in long-lived RAM state across foreground app changes and RadioLab pause/resume, while documenting that it resets on full reboot.
- [x] Keep messaging state independent of foreground UI.
- [x] Start Communicator background messaging OFF after boot.
- [x] Add volatile launcher-controlled enable/disable for the current OS session only.
- [x] Keep foreground Communicator open/close separate from background messaging service lifetime.
- [x] Make RadioLab pause/resume conditional on Communicator messaging already being ACTIVE.
- [x] Pause messaging transport while RadioLab owns the radio and resume it afterwards.
- [x] Make radio RX wake behavior configurable.
- [x] Provide experimental foreground/background messaging RX profiles as configuration.
- [ ] Validate the new messaging foundation on both physical devices.

## Communicator UX specification

Near-term first UI flow:

`Launcher -> Communicator -> known peer availability -> predefined messages -> send -> dedicated receive screen -> contextual response`

The first version is optimized for one known peer. Multi-peer contact selection may be added later only when required.

- [x] Add Communicator as the next real launcher application.
- [x] Show the known peer with a simple human-readable v0.1 label.
- [x] Show unavailable/reachable state clearly.
- [x] Prevent message actions when the known peer is unavailable.
- [x] Define the predefined-message selection screen.
- [x] Define the contextual-response selection flow.
- [x] Keep the UI explicit with 2–3 visible choices per screen; physical layout verification remains pending.
- [x] Keep chat history out of this phase.
- [ ] Revisit peer-selection UI only if a real multi-peer requirement appears.

## Preset message catalogue

Transport direction is already supported:

- `PRESET_MESSAGE + message_id`
- receiver maps `message_id` locally to visible Polish text.

This is one message type, not the permanent limit of the protocol. Future `FREE_TEXT` support remains possible.

- [x] Provide transport support for compact preset-message IDs.
- [x] Define the first five-message Communicator v0.1 catalogue.
- [x] Assign compact stable IDs to approved preset messages.
- [x] Define local Polish display text for each approved message ID.
- [ ] Define UI behavior for an unknown/unsupported message ID.

The Communicator v0.1 preset list is now fixed in the application catalogue; later additions should use new explicit IDs without reassigning existing v0.1 IDs.

## Contextual response catalogue

Transport direction is already supported:

- `PRESET_RESPONSE + response_id`
- `reference_message_id` can associate the response with the received logical message.

- [x] Provide transport support for compact preset-response IDs and a logical-message reference.
- [x] Define the Communicator v0.1 contextual response catalogue.
- [x] Define which responses are valid for each received preset message.
- [x] Assign compact stable IDs to approved responses.
- [x] Define local Polish display text for each approved response ID.

The Communicator v0.1 contextual response list is now fixed in the application catalogue; later additions should use new explicit IDs without reassigning existing v0.1 IDs.

## Device/contact availability UI

The infrastructure currently supports one known peer rather than a peer list.

- [x] Provide one-peer presence/reachability state in `messaging::Service`.
- [x] Provide latest peer RX RSSI as context.
- [x] Present reachability as available/unavailable without exposing timeout details to the UI.
- [x] Show the known peer unavailable when reachability is stale.
- [x] Show the known peer active when reachability is fresh.
- [ ] Replace the temporary single v0.1 peer label with per-device human labels only when a concrete identity/config requirement is chosen.
- [ ] Add multi-peer selection only if later requirements justify it.

## Received-message UX

When a message arrives, the intended experience is a dedicated full-screen notification/card.

- [x] Provide deduped incoming logical-message events below the UI layer.
- [x] Show the received message prominently on a dedicated screen.
- [x] Play one short ~90 ms audible alert.
- [x] Keep the received message visible until the user acts.
- [x] Show clear button guidance consistent with the current Nikoś OS interaction model.
- [x] Provide one action for contextual responses.
- [x] Allow dismiss only where the v0.1 flow permits it (notably greeting and final OK card).
- [x] Wake the display and surface Communicator from the launcher for accepted incoming traffic.

The M5StickC Plus SE has no built-in vibration motor, so current hardware feedback relies on sound and display.

## SYGNAŁ / RING behavior

The internal wire/delivery type remains RING; the user-facing Communicator action is `SYGNAŁ`.

- [x] Add RING as a Communicator wire/delivery type.
- [x] Add a separate orange `SYGNAŁ` action outside the preset catalogue and conversation state machine.
- [x] Add a custom bell glyph without emoji-font dependency.
- [x] Implement the bounded three-repeat 2.4/2.8 kHz attention pattern.
- [x] Make any normal user button immediately silence and dismiss the alert.
- [x] Keep the buzzer/animation sequence non-blocking while messaging updates continue.
- [x] Restore the previous foreground screen/state after dismissal.
- [x] Preserve retry/application-ACK/dedupe semantics so retransmitted duplicates do not repeat the alert.
- [ ] Physically validate buzzer audibility, exact perceived timing, animation readability, and dismissal responsiveness.
- [ ] Evaluate whether a cooldown/rate limit is needed only if hardware testing reveals a real product/correctness need.

## Communicator implementation

- [x] Add the Communicator foreground UI.
- [x] Connect the UI to the existing long-lived messaging service.
- [x] Implement known-peer availability presentation with derived signal bars.
- [x] Implement preset-message selection and transmission.
- [x] Implement received-message decoding into local visible strings.
- [x] Implement the dedicated receive notification UI.
- [x] Implement contextual preset responses and the deterministic human-OK flow.
- [x] Treat `CZEŚĆ!` as conversation-level fire-and-forget; optional `Cześć!` reply is terminal and requires no human OK.
- [x] Implement separate user-facing `SYGNAŁ` attention behavior using the existing internal RING delivery type.
- [x] Keep retry/dedupe/delivery semantics in `messaging::Service`, not in the UI.
- [x] Avoid chat history and avoid expanding this into a generic messaging framework without a proven requirement.

## Messaging RX power experiment

**Status: IMPLEMENTED AS EXPERIMENTAL CONFIGURATION, NOT YET HARDWARE-VALIDATED.**

Reference continuous-RX baseline:
- pre-Communicator field PoC firmware `dfb5f96b7085e7fe8328964bcd529b457e5e47d2`

Current experimental messaging profiles:

**FOREGROUND**
- interval: approximately 1000 ms
- wake window: approximately 500 ms
- reachability timeout: approximately 7000 ms

**BACKGROUND**
- interval: approximately 3000 ms
- wake window: approximately 500 ms
- reachability timeout: approximately 20000 ms

These values are configuration, not permanent product settings.

- [x] Make ESP-NOW RX power behavior configurable in `radio`.
- [x] Add the foreground 1000/500 messaging profile.
- [x] Add the background 3000/500 messaging profile.
- [x] Use configurable profile-aware reachability timeouts: approximately 7000 ms foreground and 20000 ms background.
- [x] Enable disconnected-STA connectionless power saving in project configuration.
- [ ] Compare the messaging profiles against the continuous-RX field baseline on hardware.
- [ ] Consider reducing wake window toward approximately 300 ms only if reliability remains acceptable.

## Power test metrics

For each relevant profile, compare:

- application-level delivery success;
- retries per delivered message;
- median latency;
- P95 latency;
- worst observed latency;
- cold discovery/presence time;
- battery drain / battery voltage trend;
- strong-RF behavior;
- marginal NLOS behavior.

RSSI is useful context for the RF condition, not a direct power-saving success metric.

- [ ] Define a repeatable hardware test procedure.
- [ ] Record application-level delivery success.
- [ ] Record retries per delivered message.
- [ ] Record median latency.
- [ ] Record P95 latency.
- [ ] Record worst observed latency.
- [ ] Record cold discovery/presence time.
- [ ] Record battery drain / battery voltage trend.
- [ ] Repeat tests in a strong-RF position.
- [ ] Repeat tests in a marginal NLOS position.
- [ ] Record RSSI only as environmental/radio context.

## Background communication policy

Long-term requirement: Communicator messages should ideally remain receivable while the user is in the launcher, Minutnik, another application, or Communicator itself.

The first technical boundary is now implemented; final product timing/power policy remains undecided.

- [x] Keep messaging service state independent of foreground UI.
- [x] Keep messaging transport active in the launcher.
- [x] Define an explicit pause/resume handoff for RadioLab.
- [ ] Extend foreground application orchestration so future sibling applications coexist with background messaging.
- [ ] Define how incoming communication interrupts or overlays another application.
- [ ] Validate experimental RX schedules before choosing permanent policy.
- [ ] Avoid locking interval/window values into architecture before hardware evidence.

### Future active-boost hypothesis

After static duty behavior is validated, consider:

- after TX/RX activity, keep RX fully or more frequently active for roughly 3–5 seconds;
- then return to the lower-power schedule.

- [ ] Decide whether active boost is justified by hardware results.
- [ ] If justified, compare it against the best static schedule.
- [ ] Keep it a small transport experiment; do not build a large power-management framework around it.

## Display / idle behavior

Direction only; exact timeouts are intentionally undecided.

- [ ] Define an inactivity model suitable for launcher and applications.
- [ ] Dim the display after inactivity.
- [ ] Turn the display off after longer inactivity.
- [ ] Keep communication availability independent of display state where practical.
- [x] Allow incoming Communicator traffic to wake the display and play an alert.
- [ ] Choose timeout values only after hardware/usability testing.

## Entertainment session limits

Communicator and utility applications should not use restrictive session limits.

For games/entertainment on the Nikoś profile:

- [ ] Define a bounded game-session policy.
- [ ] Return to the launcher when a session limit expires.
- [ ] Decide whether reopening requires a short break.
- [ ] Choose exact durations later.
- [ ] Keep limits scoped to entertainment rather than Communicator/utilities.
- [ ] Avoid streaks, daily rewards, random reward loops, and pull-back notifications.

## Hardware validation

- [ ] Build the Communicator infrastructure branch with ESP-IDF 5.5.5.
- [ ] Flash the same firmware to both M5StickC Plus SE units.
- [ ] Verify background presence and one-peer reachability.
- [ ] Verify latest peer RSSI updates from valid Communicator traffic.
- [ ] Verify PRESET_MESSAGE retry continues until application ACK.
- [ ] Verify PRESET_RESPONSE retry continues until application ACK.
- [ ] Verify RING retry continues until application ACK.
- [ ] Verify duplicate logical messages are ACKed but create only one incoming event during one boot.
- [ ] Verify dedupe survives launcher/application changes and RadioLab messaging pause/resume.
- [ ] Verify/document expected reboot behavior: an outstanding sender retry may surface again after receiver reboot.
- [ ] Verify messaging remains active in launcher.
- [ ] Verify entering RadioLab pauses messaging and preserves RadioLab behavior.
- [ ] Verify exiting RadioLab resumes messaging with retained messaging state.
- [ ] Run foreground/background RX schedule tests against the continuous-RX baseline.
- [ ] Validate behavior in both strong-RF and marginal NLOS positions.

## Archival intent

This checklist is intentionally temporary. Once the planned phase is substantially complete, move it to an archive/history location if it is still useful as project history. Permanent conclusions belong in the authoritative architecture/decision documentation.
