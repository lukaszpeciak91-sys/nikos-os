# Nikoś OS

Nikoś OS is a small modular embedded platform for the M5Stack M5StickC Plus SE, based on the ESP32-PICO-D4.

The project uses native ESP-IDF. The repository name remains ASCII: `nikos-os`.

The conceptual architecture is:

`BOOT -> PLATFORM / CORE -> lightweight launcher -> applications`

The first real application is RadioLab. RadioLab and the future Nikoś Communicator are sibling applications built on reusable platform services rather than defining the platform itself.

## Launcher

Boot now shows a temporary `Nikoś OS` text splash for about 0.75 s and then opens a small launcher.

Launcher entries:

1. RadioLab
2. Minutnik
3. Rozrywka

Only RadioLab opens in this milestone. Minutnik and Rozrywka are visible placeholders.

Launcher controls:

- Primary short: open/confirm the selected item.
- Secondary short: move to the next item.
- Secondary long: back where applicable; the top-level launcher has no parent screen.
- The separate power button remains outside application navigation.

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
