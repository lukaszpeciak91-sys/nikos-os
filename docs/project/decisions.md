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

For the first implementation it supports one known peer, discovery/recent-RX status, latest peer RSSI, one outstanding outgoing logical message, bounded retry until matching application ACK, and receiver dedupe. Peer identity is distinct from recent-RX age: a known peer remains eligible for bounded delivery after recent-RX status becomes stale, while the logical delivery deadline and attempt budget remain authoritative. Duplicate copies are ACKed again but do not produce duplicate notification events.

Presence is discovery-oriented rather than a continuous liveness heartbeat. Broadcast discovery retains a small bounded configurable jitter only while no peer is known; a received broadcast Presence schedules one one-shot unicast Presence reply.

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


## D-014 — Whole-device shutdown is launcher-requested and board-owned

**Status:** Accepted

The final launcher entry `WYŁĄCZ` requests whole-device shutdown only after an explicit `NIE/TAK` confirmation with `NIE` selected by default.

The launcher owns only the confirmation UI and returns a high-level shutdown action. `app_main` performs orderly runtime cleanup: it stops tones, clears volatile Communicator state, stops messaging, and ensures radio transport cleanup. The board layer then performs hardware shutdown through M5Unified.

The board implementation uses `M5.Power.powerOff()` from pinned M5Unified 0.2.22. Launcher and `app_main` do not access AXP192 directly.

This remains distinct from:
- Communicator `POWRÓT`: close foreground panel only;
- Communicator `WYŁĄCZ`: stop the background Communicator service only;
- launcher `WYŁĄCZ`: power off the whole device.

**Rationale:** Destructive whole-device power control requires explicit confirmation and orderly system cleanup while preserving the existing ownership boundary for M5-specific hardware.


## D-015 — Communicator radio mode is volatile session configuration

**Status:** Accepted

Communicator exposes one minimal radio option:
- `STANDARD` -> `radio::Mode::Normal`
- `LR` -> `radio::Mode::Lr`

The option is applied through `messaging::Service`, never directly from Communicator UI to `radio`.

Default after full boot is STANDARD. The selection is volatile and is not persisted in NVS. It survives foreground Communicator exit/re-entry, RadioLab pause/resume, and Communicator OFF -> ON within the same OS boot.

A successful change uses the existing `RadioService::set_mode()` path without restarting the messaging transport. It updates the messaging mode configuration, clears learned peer registration/reachability/RSSI, and forces fresh PRESENCE discovery while preserving:
- current foreground/background RX profile and schedule;
- presence interval/jitter;
- retry/delivery policy;
- incoming queue;
- receiver dedupe state;
- outstanding logical message and MessageId.

**Rationale:** This allows controlled STANDARD-vs-LR validation under the real duty-cycled Communicator policy while holding messaging timing and delivery semantics constant.


## D-016 — SYGNAŁ sound selection is volatile and shares one player

**Status:** Accepted

Settings v1 introduces the first real Nikoś OS setting: the Communicator `SYGNAŁ` sound.

The selected value lives in a small composition-owned `settings::State` and is one of:
- `Gentle` / `Łagodny` — default after boot;
- `Classic` / `Klasyczny`;
- `Pager`.

The setting is volatile for the current OS boot and is not persisted in NVS.

A small non-blocking `signal_sound::Player` owns the three fixed product patterns. Both Settings preview and received Communicator RING/`SYGNAŁ` playback use the same player and the same definitions. The player is advanced from the normal main loop and delegates actual buzzer hardware calls to `board`.

The ordinary short incoming-message notification tone remains separate and unchanged.

**Rationale:** This establishes one real user preference and one reusable SYGNAŁ playback source without prematurely introducing generic settings persistence, a theme system, or a generic audio/notification framework.


## D-017 — Runtime visual themes extend the existing boot-scoped settings state

**Status:** Accepted

Settings v2 extends the existing composition-owned `settings::State` with one typed visual-theme value:

- `Nikos` / Nikoś — default after boot;
- `Bursztyn`;
- `Graphite` / Grafit;
- `Lava`;
- `Matrix`.

The selection is volatile for the current OS boot and is not persisted in NVS.

Five fixed compile-time palettes live behind a small `ui_theme` boundary. Applications do not know RGB565 values and do not branch on the active theme. They request semantic `board::DisplayColor` roles, and the board layer resolves theme-varying roles through the active palette.

For the current physical-LCD validation pass, Nikos, Bursztyn, Grafit, Lava, and Matrix each define meaningfully different values for every normal palette role. These exact palette values remain experimental pending hardware validation and are not frozen as product identity.

Theme-varying roles are background, surface, primary text, secondary text, and visual accent. Product-semantic status/attention/error roles remain independent of the selected theme: active/reachable stays restrained mint/green, Communicator `SYGNAŁ` stays orange, and danger/error remains distinct from attention.

Changing the theme updates `settings::State`, switches the board palette immediately, and redraws the current Theme screen. No Save/Apply step, generic styling engine, per-screen palette, or persistence is introduced.

**Rationale:** The existing boot-scoped settings state is already the correct ownership boundary for real user preferences. A small semantic color cleanup prevents palette-specific names from leaking into applications while keeping runtime theming mechanically simple.


## D-018 — Communicator logical delivery is bounded and measurable

**Status:** Accepted

Communicator logical delivery remains application-ACK based, but retries are no longer indefinite. One outgoing logical message has a configurable experimental retry interval, bounded retry jitter, maximum send-attempt budget, and absolute logical delivery timeout.

The first hardware-test defaults are approximately:
- 1000 ms retry interval;
- up to 250 ms additional retry jitter;
- 8 send attempts;
- 12000 ms absolute logical delivery timeout.

These values are experimental tuning inputs, not permanent product policy.

The first attempt is immediate when transport and a peer identity are available. Retries keep the same logical MessageId. Recent-RX/reachability age does not suppress attempts to a known peer; if that peer is actually unavailable, MAC-result pacing, the attempt budget, application-ACK requirement, and logical timeout produce the finite Failed outcome. Peer silence never resets the deadline or attempt budget. A deliberate `messaging.pause_transport()` handoff to RadioLab is different: while RadioLab intentionally owns the radio, logical delivery timeout and retry-delay clocks are suspended. On resume the same logical MessageId, send-attempt count, and remaining delivery/retry timing budget are preserved. A matching application ACK is the only authoritative `Delivered` outcome. Attempt/deadline exhaustion produces an explicit `Failed` outcome and clears the outgoing logical delivery.

Development metrics record logical delivery kind/outcome, ESP-NOW send-request attempts, immediate send-request failures, logical delivery latency, and cumulative accepted send submissions for logical payloads, application ACKs, and Presence. These are submission-level measurements, not true PHY-level Wi-Fi transmission counts.

This bounded-delivery decision remains unchanged by later discovery-oriented Presence pacing, the protocol-v2 payload encoding, RX duty-cycle schedules, or TxResult attribution.

**Rationale:** Hardware testing showed that indefinite retransmission can waste sender energy and leave UI state waiting forever. Bounded delivery provides a safe measurement baseline before deeper MAC-aware or Presence optimization.


## D-019 — ESP-NOW TxResult is a serialized pacing signal, not delivery

**Status:** Accepted

`messaging::Service` now consumes ESP-NOW TxResult only for sender pacing and measurement. Application ACK remains the sole authoritative `Delivered` condition.

Messaging peer-unicast submissions are serialized so at most one outgoing logical payload, application ACK, or one-shot unicast Presence reply is awaiting a peer TxResult at a time. Application ACK requests that arrive while this slot is busy are retained in a fixed four-entry pending-ACK queue and are submitted before a due outgoing logical retry when the slot becomes free. Queue overflow is logged; sender retry plus receiver dedupe remains the recovery path.

For an accepted logical payload submission:
- MAC FAIL schedules the existing experimental 1000 ms + 0…250 ms jitter retry;
- MAC SUCCESS does not deliver the logical message; it starts an ACK grace derived from the current local RX interval plus a 250 ms experimental margin, giving approximately 1250 ms in foreground and 3250 ms in background;
- a missing TxResult is guarded for 500 ms. Guard expiry is counted separately and forces a messaging radio transport reset before any newer peer unicast is submitted, preventing a late destination-only callback from being attributed to a newer transmission.

If an application ACK arrives before the payload TxResult is processed, logical delivery completes immediately as `Delivered`, but the physical unicast attribution remains reserved until the matching TxResult or missing-result guard resolves. This prevents the later callback from affecting a newer logical message.

If the missing-TxResult attribution-barrier radio restart itself fails, messaging fails closed rather than behaving like a RadioLab pause: any still-active logical delivery completes as `Failed`, pending unicast/ACK work and stale reachability are cleared, and new logical sends remain rejected until the normal Communicator service lifecycle is restarted with `stop()` followed by a fresh `begin()`. An outgoing message already completed as `Delivered` is not converted to `Failed`.

RadioLab's deliberate transport handoff still freezes logical delivery/retry timing. Any peer unicast whose callback remains unresolved at the handoff is conservatively closed as missing before radio ownership is transferred; the same logical MessageId and attempt count remain, and the resulting retry timing is frozen until resume.

The TxResult pacing rules remain unchanged when Presence becomes discovery-oriented or the application payload moves to protocol v2. RX duty schedules, recent-RX timeouts, Wi-Fi power-save mode, and application ACK/dedupe semantics remain unchanged.

**Rationale:** Destination MAC plus success/failure is insufficient to distinguish an outgoing logical payload from an application ACK to the same peer. Serializing only messaging unicast traffic gives deterministic TxResult ownership while allowing MAC success to reduce blind duplicate retransmission without weakening application-level delivery semantics.


## D-020 — Communicator Presence is discovery-oriented

**Status:** Accepted

Communicator Presence is a peer-discovery mechanism, not a continuous liveness heartbeat.

While no peer MAC is known, messaging sends broadcast Presence immediately when the active service begins updating and then at the existing experimental 2000 ms interval plus 0…250 ms jitter. Once a compatible peer is learned, normal periodic broadcast Presence stops and an idle known-peer session produces no recurring Presence traffic.

Peer identity and recent-RX status are separate concepts. Any valid received peer traffic continues to refresh recent-RX/RSSI metadata, but a known peer remains eligible for preset, response, and RING bounded delivery after that recent status becomes stale. The UI uses a neutral known-but-stale state rather than claiming the peer is unavailable.

A valid broadcast Presence learns/confirms the one peer and schedules one lightweight unicast Presence reply through the existing serialized messaging-unicast slot. Application ACK has first priority, the pending Presence reply second, and a due logical payload third. The receiver distinguishes discovery from reply using `radio::RxEvent::destination`: broadcast destination schedules the reply; unicast Presence does not, preventing a Presence-response echo.

A Presence reply has no application-level ACK or retry protocol. Its MAC TxResult is attributed through the same single in-flight slot as other messaging unicasts, and the existing missing-TxResult guard/attribution-barrier recovery applies unchanged.

Communicator OFF clears volatile peer identity. Fresh ON starts discovery again. Radio-mode change clears peer identity and restarts discovery in the selected mode. RadioLab pause/resume preserves learned identity and emits no discovery traffic while transport is intentionally paused. No peer MAC is persisted.

**Rationale:** Communicator usage is sparse and transactional. Continuous heartbeat traffic after discovery spends energy without being required for message delivery; the existing bounded delivery model is the correct mechanism for determining whether a known peer can actually receive a transaction.


## D-021 — Communicator protocol v2 uses compact type-specific frames

**Status:** Accepted

Communicator protocol v2 replaces the fixed 20-byte v1 application frame with an explicit variable-length type-specific format. Both controlled M5Stick devices must run the same v2 firmware; there is no v1 fallback, negotiation, capability exchange, or compatibility mode.

The common v2 header is two bytes: byte 0 is the fixed v2 discriminator `0xA7`; byte 1 is the existing numeric MessageType. The discriminator itself identifies protocol v2, so version and type are not bit-packed. Unknown discriminator, unknown type, truncated data, or any extra trailing byte causes decode rejection.

Wire sizes are:

| Type | Size |
| --- | ---: |
| Presence | 2 B |
| Ring | 6 B |
| ACK | 6 B |
| PresetMessage | 8 B |
| PresetResponse | 12 B |

Presence carries only the header. Ring carries its 32-bit logical MessageId. ACK carries only the 32-bit logical MessageId being acknowledged and has no independent logical MessageId. PresetMessage carries a 32-bit logical MessageId plus 16-bit PresetId. PresetResponse carries its 32-bit logical MessageId, the 32-bit referenced message ID, and 16-bit ResponseId. All multi-byte IDs remain big-endian.

PresetId and ResponseId are stable 16-bit semantic catalogue identifiers, never UI row/option indexes. The local catalogue remains responsible for mapping IDs to local display text; human-readable strings are never sent over ESP-NOW. Keeping the full uint16_t wire representation avoids an artificial 255-value ceiling and allows catalogue growth without another protocol redesign.

The 32-bit logical MessageId is deliberately retained. This protocol change reduces only the application payload bytes passed to the radio; bounded delivery, application-ACK authority, retry/deadline policy, TxResult pacing/serialization, discovery-oriented Presence behavior, RX schedules, STANDARD/LR behavior, RadioLab ownership, and UI state machines remain unchanged.

**Rationale:** Communicator exchanges predefined semantic IDs, so carrying fields that are unused by a given message type wastes application payload bytes without improving delivery semantics.


## D-022 — Communicator human flow is shallow and technical ACK is not a human OK

**Status:** Accepted

On the physical 240×135 device, Communicator presents one dominant message/choice at a time. The main screen keeps one linear focus sequence—Preset 1..5, SYGNAŁ, OPCJE, POWRÓT—with secondary short = next and primary short = select. Response choice follows the same NEXT -> SELECT model and shows exactly one selectable response while retaining the received preset for context.

Application ACK remains the sole technical delivery acknowledgement. Normal human conversation no longer emits or waits for a separate mandatory HumanOk response. After sending a normal response, the responder waits for the technical delivery receipt of that response. If it is Delivered, the UI returns to Main unless exactly one `SuspendedWaitingContext` exists from simultaneous-preset collision, in which case that prior WaitingForResponse state is restored automatically. A Failed receipt keeps the existing explicit delivery-failure UX and does not pretend the response succeeded.

For resolved responses, the initiator displays the received response and dismisses it locally without sending another message. For intentionally unresolved responses—`ZA CHWILĘ`, `PÓŹNIEJ`, `SPRAWDZĘ`—the explicit `ZACZEKAĆ?` branch remains because it has conversational meaning. Primary sends the existing Wait preset; secondary closes locally without transmitting anything. The final `TAK, ZACZEKAJ` / `NIE CZEKAJ` response ends the human conversation without a HumanOk.

The deterministic MAC-based simultaneous-preset arbitration and the single suspended-conversation slot remain unchanged in scope. Transport/application ACK, bounded retry, TxResult pacing, Presence discovery, protocol v2, RX schedules, STANDARD/LR, and RadioLab ownership are not redesigned by this decision.

**Rationale:** Physical LCD testing showed that multiple simultaneous rows and a mandatory human OK create visual and conversational overhead on a sparse transactional communicator. Technical delivery and human conversational meaning should remain separate.


## D-023 — Native M5GFX typography and ASCII-first v0.1 copy after hardware readability failure

**Status:** Accepted for v0.1 hardware validation

The embedded custom Polish UI font experiment is not used for normal Launcher or Communicator product UI. Physical M5StickC Plus SE testing showed that the small generated source font became visibly broken/pixelated when scaled, including on ASCII-only words, so normal product UI returns to the native M5GFX `Font0` rendering path.

For this validation phase, interactive product copy is ASCII-first and avoids Polish diacritics. The custom font resource may remain in the board component for isolated experimentation/sanity checks, but normal product screens must not depend on it.

This does not change communication semantics or localization ownership. PresetId and ResponseId remain stable language-independent semantic IDs, human-readable text remains local to each device, and no display strings are sent over ESP-NOW. The ASCII-first copy is a temporary hardware-readability decision rather than a permanent localization architecture.

**Rationale:** On a 240×135 physical LCD, readable native typography is more important than preserving diacritics through a custom font that degrades all text.


## D-024 — Display lifecycle is independent from Communicator/device lifecycle

**Status:** Accepted for hardware validation

Nikoś OS display lifecycle v0.1 has three product states: `Active`, `Dimmed`, and `DisplayOff`. The experimental policy dims after 15 seconds and turns the LCD/backlight off after 45 seconds from the same last meaningful activity timestamp.

`DisplayOff` does not stop the main loop, messaging service, Communicator background reception, or ESP-NOW RX scheduling. The first physical user-button gesture from `DisplayOff` wakes to normal brightness and is consumed until the involved user buttons are released, so the wake gesture cannot also activate UI behavior. A button gesture from `Dimmed` restores normal brightness but continues through normal application handling.

Accepted user-visible Communicator messages and received SYGNAL wake/reset the lifecycle at Communicator semantic acceptance points. Transport-internal Presence, application ACK, retry, TxResult, reachability, and dedupe activity do not count as display activity.

The `power::DisplayLifecycle` component owns only display state/timing/wake-input policy. Its filtered-input result carries a one-shot `WakeReason::UserButton` only for a physical wake from `DisplayOff`; `app_main` consumes that reason as the explicit extension point for the approved future Clock Glance. Normal communication wake through `note_visible_activity()` never produces the user-button reason. `board` owns M5GFX-specific brightness/sleep/wakeup operations, and `app_main` coordinates filtered input with the active foreground application.

Clock Glance itself is not implemented in this decision/PR: there is no RTC, time rendering, HH:MM UI, or glance timer. The approved future behavior is a short (~4 s) Clock Glance that returns directly to `DisplayOff` unless a second fresh user gesture dismisses it; that second gesture will be consumed centrally by the future Clock implementation rather than passed to the previously focused app.

No automatic whole-device shutdown, light/deep sleep, CPU sleep, battery power policy, NVS setting, event bus, or extra task is introduced.

**Rationale:** LCD/backlight savings can be validated independently without risking the already working Communicator transport/session lifecycle.

