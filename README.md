# Nikoś OS

Nikoś OS is a small modular embedded platform for the M5Stack M5StickC Plus SE, based on the ESP32-PICO-D4.

The project targets native ESP-IDF as its primary framework direction. The repository name remains ASCII: `nikos-os`.

The conceptual architecture is:

`BOOT -> PLATFORM / CORE -> lightweight launcher -> applications`

The first real application is RadioLab. RadioLab and the future Nikoś Communicator are sibling applications built on reusable platform services rather than defining the platform itself.

## Project documentation

- [Architecture](docs/project/architecture.md)
- [Decisions](docs/project/decisions.md)
- [Progress](docs/project/progress.md)
- [Workflow](docs/project/workflow.md)
- [Agent working rules](AGENTS.md)
