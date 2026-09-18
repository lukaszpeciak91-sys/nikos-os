# Nikoś OS

Nikoś OS is a small modular embedded platform for the M5Stack M5StickC Plus SE, based on the ESP32-PICO-D4.

The project targets native ESP-IDF as its primary framework direction. The repository name remains ASCII: `nikos-os`.

The conceptual architecture is:

`BOOT -> PLATFORM / CORE -> lightweight launcher -> applications`

The first real application is RadioLab. RadioLab and the future Nikoś Communicator are sibling applications built on reusable platform services rather than defining the platform itself.

## Hardware sanity milestone

The initial firmware scaffold is intentionally limited to hardware bring-up.

Pinned toolchain and hardware dependencies:

- ESP-IDF 5.5.5
- M5Unified 0.2.22
- M5GFX 0.2.29

M5Unified and M5GFX are resolved through the ESP-IDF Component Manager using exact versions declared in `main/idf_component.yml`. The default target is ESP32 with 4 MB flash, matching the M5StickC Plus SE hardware.

Build with an ESP-IDF 5.5.5 environment:

```sh
idf.py build
idf.py -p <PORT> flash monitor
```

The sanity screen reports the firmware version, M5Unified board identification, Wi-Fi station MAC address, battery information, Button A/B event counters, the most recent button event, and whether the startup buzzer command was accepted. Physical verification on the actual Plus SE remains required for LCD output, A/B mapping, PMU readings, and audible buzzer output.

## Project documentation

- [Architecture](docs/project/architecture.md)
- [Decisions](docs/project/decisions.md)
- [Progress](docs/project/progress.md)
- [Workflow](docs/project/workflow.md)
- [Agent working rules](AGENTS.md)
