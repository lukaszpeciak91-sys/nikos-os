# Next Phase Plan

> **Temporary implementation-phase checklist**
>
> This file tracks the currently agreed near-term Nikoś OS product and technical direction. It is operational planning, not permanent architecture.
>
> Check items off as they are actually implemented and verified. When this phase is substantially complete, this file may be moved to an archive/history location. Permanent architectural decisions must continue to live in the authoritative project documentation.

## Communicator UX specification

Target product flow:

`Launcher -> Communicator -> radio/discovery active -> available devices -> reachable peer -> predefined messages -> send -> dedicated receive screen -> contextual response`

- [ ] Define the first Communicator launcher entry and application lifecycle.
- [ ] Define the simple device/contact list UI.
- [ ] Support simple human-readable peer labels such as `Nikoś` and `Łuki`.
- [ ] Show unreachable peers as unavailable / greyed out.
- [ ] Prevent selection of unreachable peers.
- [ ] Show reachable peers as visually active and selectable.
- [ ] Define the predefined-message selection screen.
- [ ] Define the contextual-response selection flow.
- [ ] Keep the UI explicit and small enough for the M5StickC Plus SE display.
- [ ] Confirm that no chat-history model is required for this phase.

## Preset message catalogue

Initial transport direction:

- `PRESET_MESSAGE + message_id`
- receiver maps `message_id` locally to the visible Polish text.

This is one message type, not the permanent limit of the protocol. Future `FREE_TEXT` support remains possible.

- [ ] Design the first predefined-message catalogue separately from protocol plumbing.
- [ ] Assign compact stable IDs to the first approved preset messages.
- [ ] Define local Polish display text for each approved message ID.
- [ ] Define behavior for an unknown/unsupported message ID.
- [ ] Keep future free-text support possible without redesigning the preset-message concept.

The exact message list is intentionally not finalized in this document.

## Contextual response catalogue

Initial response direction:

- `PRESET_RESPONSE + response_id`
- receiver maps `response_id` locally to visible Polish text.

- [ ] Design the first contextual response catalogue.
- [ ] Define which responses are valid for each received preset message.
- [ ] Assign compact stable IDs to approved responses.
- [ ] Define local Polish display text for each approved response ID.
- [ ] Keep response handling small and explicit rather than creating a generic conversation engine.

The exact response list is intentionally not finalized in this document.

## Device/contact availability UI

- [ ] Define the minimum peer availability state needed by Communicator.
- [ ] Reuse real radio/discovery evidence rather than inventing a Bluetooth-like connected state.
- [ ] Define the freshness threshold used only for Communicator availability presentation.
- [ ] Show unreachable peers as unavailable and non-selectable.
- [ ] Show reachable peers as active and selectable.
- [ ] Confirm how human-readable labels are associated with known devices for the first implementation.

## Received-message UX

When a message arrives, the intended experience is a dedicated full-screen notification/card.

- [ ] Show the received message prominently on a dedicated screen.
- [ ] Play a short audible alert.
- [ ] Keep the received message visible until the user acts.
- [ ] Show clear button guidance consistent with the current Nikoś OS interaction model.
- [ ] Provide one action for contextual responses.
- [ ] Provide one action to dismiss/exit as appropriate.
- [ ] Keep incoming-message handling independent of chat history.
- [ ] Confirm how incoming communication wakes or replaces the current screen.

The M5StickC Plus SE has no built-in vibration motor, so current hardware feedback relies on sound and display.

## RING behavior

Plan a simple `RING` command for getting the peer's attention from another room.

- [ ] Define the minimum RING command semantics.
- [ ] Define a noticeable but short sound pattern.
- [ ] Make RING easy to silence.
- [ ] Prevent continuous ringing.
- [ ] Keep the implementation bounded to avoid unnecessary battery drain.
- [ ] Evaluate whether a cooldown/rate limit is needed after initial testing.

## Communicator implementation

- [ ] Add Communicator as the next real Nikoś OS application.
- [ ] Activate radio/discovery when entering Communicator.
- [ ] Implement the device/contact availability list.
- [ ] Implement reachable-peer selection.
- [ ] Implement preset-message selection and transmission by compact ID.
- [ ] Implement received-message decoding and dedicated notification UI.
- [ ] Implement contextual preset responses.
- [ ] Implement RING after the basic message flow is stable.
- [ ] Verify Communicator remains a sibling application rather than a platform dependency.
- [ ] Avoid introducing chat history, retries, or a generic messaging framework unless later requirements prove they are needed.

## RadioLab power-save experiment

**Status: NOT YET VALIDATED.**

Current baseline: RadioLab keeps RX effectively continuous.

The following values are experiment profiles only. They are not permanent Communicator or platform settings.

### Comparison profiles

**A. BASELINE**
- continuous RX

**B. DUTY A**
- interval: 1000 ms
- wake window: 500 ms

**C. DUTY B**
- interval: 3000 ms
- wake window: 500 ms

Reason: measure message reliability, latency, and battery impact before choosing any background communication policy.

- [ ] Preserve a continuous-RX baseline profile for comparison.
- [ ] Add a RadioLab experiment profile for 1000 ms interval / 500 ms wake window.
- [ ] Add a RadioLab experiment profile for 3000 ms interval / 500 ms wake window.
- [ ] Keep the experiment explicit and removable rather than making these values platform defaults.
- [ ] Test all three profiles under comparable conditions.
- [ ] Consider reducing the wake window toward approximately 300 ms only if earlier profiles remain reliable.

## Power test metrics

For each relevant profile, compare:

- application-level delivery success;
- retries per delivered message;
- median latency;
- P95 latency;
- worst observed latency;
- cold discovery time;
- battery drain / battery voltage trend;
- strong-RF behavior;
- marginal NLOS behavior.

RSSI is useful context for the RF condition, not a direct power-saving success metric.

- [ ] Define a repeatable hardware test procedure.
- [ ] Record application-level delivery success.
- [ ] Record retries per delivered message where retries exist in the tested experiment.
- [ ] Record median latency.
- [ ] Record P95 latency.
- [ ] Record worst observed latency.
- [ ] Record cold discovery time.
- [ ] Record battery drain / battery voltage trend.
- [ ] Repeat tests in a strong-RF position.
- [ ] Repeat tests in a marginal NLOS position.
- [ ] Record RSSI only as environmental/radio context.

## Background communication policy

Long-term product requirement: Communicator messages should ideally remain receivable while the user is in:

- launcher;
- Minutnik;
- another application;
- Communicator itself.

This implies some periodic background ESP-NOW receive availability, but the final power policy is intentionally undecided.

- [ ] Validate static duty-cycle behavior in RadioLab before designing the product policy.
- [ ] Define the minimum platform/application boundary needed for background receive availability only after experiment results exist.
- [ ] Decide how launcher and sibling applications coexist with background communication.
- [ ] Decide how incoming communication is surfaced while another application is active.
- [ ] Avoid locking permanent interval/window values into architecture before measurement.

### Future active-boost hypothesis

After static duty-cycle behavior is validated, consider a small follow-up experiment:

- after TX/RX activity, keep RX fully or more frequently active for roughly 3–5 seconds;
- then return to the low-power schedule.

- [ ] Decide whether the active-boost experiment is justified by static-duty test results.
- [ ] If justified, compare active boost against the best static low-power profile.
- [ ] Keep this as a local experiment; do not build a large power-management framework around it.

## Display / idle behavior

Direction only; exact timeouts are intentionally undecided.

- [ ] Define an inactivity model suitable for launcher and applications.
- [ ] Dim the display after a period of inactivity.
- [ ] Turn the display off after a longer period of inactivity.
- [ ] Keep communication availability independent of display state where practical.
- [ ] Allow incoming communication to wake the display.
- [ ] Allow incoming communication to play its alert while the display was dim/off.
- [ ] Choose actual timeout values only after hardware/usability testing.

## Entertainment session limits

Nikoś OS may later include simple games or entertainment, but communication and useful tools remain the product focus.

Communicator and utility applications should not use restrictive session limits.

For games/entertainment on the Nikoś profile:

- [ ] Define a bounded game-session policy.
- [ ] Return to the launcher when a game session limit expires.
- [ ] Decide whether reopening should require a short break.
- [ ] Choose exact session/break durations later; do not hard-code them from this plan.
- [ ] Keep session limiting scoped to entertainment rather than Communicator/utilities.
- [ ] Avoid streaks.
- [ ] Avoid daily rewards.
- [ ] Avoid random reward loops.
- [ ] Avoid notifications whose only purpose is pulling the user back into a game.

Purpose:
- preserve battery;
- avoid encouraging prolonged device use;
- keep Nikoś OS focused on communication and useful tools.

## Hardware validation

- [ ] Build and flash the same current firmware to both M5StickC Plus SE units before new radio experiments.
- [ ] Verify the current launcher/RadioLab lifecycle remains stable before adding Communicator.
- [ ] Validate the first Communicator peer discovery/availability flow on two devices.
- [ ] Validate preset-message delivery on two devices.
- [ ] Validate received-message sound and dedicated screen behavior.
- [ ] Validate contextual responses.
- [ ] Validate RING audibility and easy dismissal.
- [ ] Run the RadioLab power-profile comparison using the defined metrics.
- [ ] Validate behavior in both strong-RF and marginal NLOS positions.
- [ ] Validate display dim/off/wake behavior once implemented.
- [ ] Revisit the checklist after hardware evidence and move permanent conclusions into the appropriate authoritative documentation.

## Archival intent

This checklist is intentionally temporary. Once the planned phase is substantially complete, move it to an archive/history location if it is still useful as project history. Do not leave tentative implementation hypotheses here as substitutes for permanent architectural decisions.
