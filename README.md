# Nikoś OS

Nikoś OS is a small modular embedded platform for the M5Stack M5StickC Plus SE, based on the ESP32-PICO-D4.

The project uses native ESP-IDF. The repository name remains ASCII: `nikos-os`.

The conceptual architecture is:

`BOOT -> PLATFORM / CORE -> lightweight launcher -> applications`

RadioLab and Communicator are the first two real sibling applications, built on reusable platform services rather than defining the platform itself.

## Launcher

Boot now shows a short deterministic branded splash: sparse signal fragments align into the `NIKOS` wordmark, the small green `OS` mark appears last, and the final logo holds for about 0.75 s before the launcher.

Launcher entries:

1. Communicator
2. RadioLab
3. Minutnik
4. Rozrywka
5. WYŁĄCZ

Communicator and RadioLab are real applications. Minutnik and Rozrywka remain visible placeholders. The final `WYŁĄCZ` entry is whole-device shutdown and opens a `NIE/TAK` confirmation with `NIE` selected by default. The launcher remains a fixed static list rather than an app registry/plugin system. Communicator background messaging starts OFF after boot and is enabled explicitly for the current OS session from the launcher. The splash and launcher use a dark navy shell, light typography, and a restrained green accent. The launcher also shows a small cached `BAT xx%` indicator, sampled about once per second.

Launcher controls:

- Primary short: open/confirm the selected item.
- Secondary short: move to the next item.
- Secondary long: back where applicable; the top-level launcher has no parent screen.
- The separate power button remains outside application navigation.

## Communicator v0.1 core UX

### Communicator session lifecycle

Communicator background messaging is **OFF after boot**.

The enabled/disabled state is volatile for the current OS session only and is not stored in NVS.

Launcher behavior:
- the Communicator row keeps its normal label and shows a small right-side state indicator;
- open circle = OFF;
- calm green solid circle = ACTIVE;
- selecting Communicator while OFF shows `WŁĄCZYĆ KOMUNIKATOR?` with M5/PRIMARY = `TAK` and SIDE/SECONDARY = `NIE`;
- selecting Communicator while ACTIVE shows `KOMUNIKATOR AKTYWNY` with `WEJDŹ` selected by default and `WYŁĄCZ` as the second choice.

Foreground Communicator visibility and background messaging lifetime are separate:
- leaving the Communicator panel restores the background RX profile but does not disable messaging;
- choosing `WYŁĄCZ` stops messaging, clears volatile Communicator session/conversation state, and returns to the launcher;
- start -> stop -> start is supported within one OS session.

When messaging is OFF, the launcher does not inspect Communicator incoming events.

RadioLab only pauses/resumes messaging when Communicator was ACTIVE before RadioLab took exclusive radio ownership. Exiting RadioLab never starts messaging that was OFF.


Communicator uses the long-lived `messaging::Service` and the embedded Polish UI font.

The main screen shows one known peer directly, with:
- a simple v0.1 peer label;
- available/unavailable state from messaging reachability;
- a small three-level signal indicator derived from recent RSSI;
- the preset-message list without raw dBm emphasis.

Preset messages use stable explicit IDs:

1. `CZEŚĆ!`
2. `MOŻESZ GADAĆ?`
3. `IDZIESZ NA SPACER?`
4. `MASZ PUSZKI?`
5. `ZACZEKAĆ?`

Controls:
- Secondary short: next item/choice.
- Primary short: select/send/respond.
- The main Communicator panel includes a normal selectable `POWRÓT` item directly below `SYGNAŁ`.
- `POWRÓT` exits only the foreground panel and returns to the launcher; background messaging remains ACTIVE.
- `POWRÓT` remains selectable even when the peer is unavailable.
- Secondary long remains a shortcut to the same foreground-panel exit.
- Power remains outside application navigation.

The message list stays visible but inactive while the peer is unavailable. Communicator switches messaging to the experimental foreground RX profile while open and restores the background profile on exit.

Incoming preset messages/responses wake the display, play one short ~90 ms alert, and use dedicated full-screen cards. The current exchange is held only in a small volatile deterministic state machine; there is no chat history or free text.

Human-visible `OK` is a PRESET_RESPONSE used by the conversation flow and is separate from the internal application ACK used by `messaging::Service` for reliable delivery.

The `CZEŚĆ!` preset is fire-and-forget at the conversation level: the sender remains on the main screen. The receiver may dismiss it or optionally answer `Cześć!`; that optional reply is itself terminal and does not require human `OK`.

Special responses `Za chwilę`, `Później`, and `Sprawdzę` allow the initiator to choose `OK` or send the same `ZACZEKAĆ?` preset used by the main catalogue.

Incoming logical events are inspected non-destructively and are consumed from `messaging::Service` only after Communicator has retained/accepted them. Communicator may additionally retain exactly one temporarily incompatible incoming event locally, consume that event from the service queue, and continue inspecting later queued traffic needed by the active exchange. The deferred event keeps its original logical identity and is surfaced once the conversation reaches a compatible state. An occupied deferred slot is never overwritten. A full small service queue does not discard an already retained event; an unretained new logical message remains un-ACKed so the existing sender retry can deliver it later.

True simultaneous conversational initiation uses one deterministic collision rule: when both sides are waiting for a response and receive the other's new conversational preset, the device with the lexicographically lower self MAC yields. It preserves exactly one original WaitingForResponse context, handles the peer's short exchange through its normal response/human-OK flow, then restores its original wait. The higher-MAC device keeps its own exchange active and later surfaces its already deferred peer question. No protocol synchronization is added.

### SYGNAŁ attention action

Communicator exposes a separate orange `SYGNAŁ` action below the preset-message area. It is not part of the preset catalogue and does not enter the conversation state machine.

Sender behavior:
- `SYGNAŁ` is selectable only while the known peer is reachable;
- the UI uses a custom line/circle bell glyph rather than an emoji font;
- sending reuses the existing internal RING delivery type and its logical ID / retry / application-ACK / dedupe semantics.

Receiver behavior:
- a dedicated full-screen `SYGNAŁ` alert temporarily overlays the current Communicator state;
- the display is woken;
- a large custom bell and simple alternating ringing arcs are rendered;
- the buzzer plays three non-blocking repeats of:
  - 2.4 kHz for 180 ms;
  - 180 ms silence;
  - 2.8 kHz for 180 ms;
  - 500 ms silence;
- the complete pattern is approximately 3.12 seconds and does not continue after the third repeat;
- any normal user-button event immediately stops audio and dismisses the alert;
- after dismissal, the previous Communicator screen/state is restored, or the launcher is restored when the alert originally surfaced from the launcher.

The alert does not create a conversation, response, or human-OK flow. Retransmitted duplicates remain handled by messaging dedupe, so they are ACKed without reopening/restarting the alert.

## RadioLab v0.1 foundation

This firmware is intentionally identical on both test devices. RadioLab v0.1 does not assign permanent BASE or MOBILE roles: either device can stay at home or be carried during a range test.

Pinned versions:

- ESP-IDF 5.5.5
- M5Unified 0.2.22
- M5GFX 0.2.29

M5Unified and M5GFX are resolved through the ESP-IDF Component Manager using exact versions.

RadioLab uses ESP-NOW on fixed Wi-Fi channel 6. Compatible peers announce a versioned DISCOVERY packet by broadcast, learn one peer MAC automatically, and register that peer for unicast PING, ACK, and HELLO traffic. Peer persistence is intentionally not implemented; devices rediscover after reboot.

Radio modes are selected explicitly:

- `NORMAL`: 802.11 b/g/n protocol bitmap
- `LR`: Espressif LR-only protocol bitmap

There is no automatic mode switching. Both devices must be configured to the same mode for a benchmark.

### Field-test controls

Inside RadioLab:

- Secondary short: send PING immediately.
- Primary short: send HELLO immediately.
- Primary long: toggle NORMAL/LR.
- Secondary long: exit RadioLab and return to the launcher.
- Separate power button: power only; no application-navigation action.

A long press is handled as its own action and does not also trigger the corresponding short action.

The main field screen shows only:

- a green/red recent-link indicator;
- the latest valid RX RSSI from the active peer while the link is fresh;
- battery percentage;
- the active NORMAL/LR mode;
- `SIDE PING` and `M5 HELLO` hints.

Received PING messages produce a short beep and retain the existing application ACK behavior. A matching application ACK briefly shows a green `✓ PING OK` confirmation on the sender.

Received HELLO messages produce a beep, send an application ACK referencing the HELLO sequence, and switch to a latched large `HELLO` screen. A matching HELLO ACK briefly shows a green `✓ HELLO OK` confirmation on the sender. Any user button event dismisses the received HELLO screen and is consumed without triggering another action. Radio processing continues while the HELLO screen is visible, and outgoing delivery feedback does not interrupt that latched screen.

The two user-button positions were physically verified on the M5StickC Plus SE. The separate power button remains outside launcher and RadioLab navigation.

### RadioLab lifecycle

RadioLab no longer owns the whole firmware runtime. Entering it starts Wi-Fi/ESP-NOW and discovery. Exiting it stops ESP-NOW/Wi-Fi activity and clears transient RadioLab peer/session state. Re-entering starts a clean RadioLab session.

### Build and flash

Use an ESP-IDF 5.5.5 environment:

```sh
idf.py build
idf.py -p <PORT> flash monitor
```

Flash the same firmware to both devices.

Physical verification is still required for LCD orientation, passive buzzer output, battery/AXP192 readings, ESP-NOW exchange, RSSI metadata, RTT behavior, and Espressif LR behavior.

## Project documentation

- [Architecture](docs/project/architecture.md)
- [Decisions](docs/project/decisions.md)
- [Progress](docs/project/progress.md)
- [Workflow](docs/project/workflow.md)
- [Agent working rules](AGENTS.md)
