# Nikoś OS

Nikoś OS is a small modular embedded platform for the M5Stack M5StickC Plus SE, based on the ESP32-PICO-D4.

The project uses native ESP-IDF. The repository name remains ASCII: `nikos-os`.

The conceptual architecture is:

`BOOT -> PLATFORM / CORE -> lightweight launcher -> applications`

The first real application is RadioLab. RadioLab and the future Nikoś Communicator are sibling applications built on reusable platform services rather than defining the platform itself.

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

### Controls

The current logical controls are:

- Button A short: execute the selected action
- Button B short: cycle `PING -> HELLO -> LIVE -> MODE`
- Button B long: return selection to `PING`
- Button A long: unused

The actual physical A/B mapping on the M5StickC Plus SE must still be verified on the real units.

`LIVE` sends periodic PING messages. `MODE` toggles NORMAL/LR and clears the active peer so discovery restarts in the new mode.

### Build and flash

Use an ESP-IDF 5.5.5 environment:

```sh
idf.py build
idf.py -p <PORT> flash monitor
```

Flash the same firmware to both devices.

Physical verification is still required for LCD orientation, physical A/B mapping, passive buzzer output, battery/AXP192 readings, ESP-NOW exchange, RSSI metadata, RTT behavior, and Espressif LR behavior.

## Project documentation

- [Architecture](docs/project/architecture.md)
- [Decisions](docs/project/decisions.md)
- [Progress](docs/project/progress.md)
- [Workflow](docs/project/workflow.md)
- [Agent working rules](AGENTS.md)
