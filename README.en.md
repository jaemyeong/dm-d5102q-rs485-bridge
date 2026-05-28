# DM-D5102Q RS485 TCP Bridge
> 🌐 Read this in: [한국어](./README.md) · **English**

This project is a debugging and self-diagnostics tool for user-owned IoT equipment, built with an M5Stack AtomS3-Lite / Atom-Lite and Atom RS485 base to inspect DM-D5102Q wall pad RS485 traffic. It is intended to provide packet capture, manual HEX frame send, a WebSocket console, TCP bridge modes, and OTA updates.

## Status

- Current version: 0.1.9 (Unreleased)
- Build system: Arduino IDE 2.x
- Target boards: M5Stack AtomS3-Lite and M5Stack Atom-Lite
- Firmware layout: separate `firmware/atoms3-lite/` and `firmware/atom-lite/` directories
- Shared code: `shared/` is the master tree and is copied one-way into each board firmware

## Feature Scope

| PRD | Feature |
|---|---|
| §5.1-5.2 | Wi-Fi provisioning, reconnect handling, AP fallback |
| §5.3-5.8 | RS485 UART settings, RX/TX, framing, half-duplex control |
| §5.9-5.10 | TCP bridge client/server modes, reconnect, keepalive, backoff |
| §5.11-5.12 | Web console and WebSocket batching |
| §5.13-5.14 | Packet replay and baud scanner |
| §5.15-5.19 | Status LED, OTA, dashboard, overflow, factory reset |

- One-click reboot from the admin header; Wi-Fi/queue/auth alerts surfaced automatically.

## Quick Start

1. Install Arduino IDE 2.x and the ESP32 board package.
2. Install the recommended libraries: `M5Atom`, `M5AtomS3`, `ESPAsyncWebServer`, `AsyncTCP`, `Preferences`, `ElegantOTA`, and `ArduinoJson`.
3. In the target board directory, copy `secrets.h.example` to `secrets.h` and configure Wi-Fi/AP values. These files will be added in a later implementation phase.
4. Compile and upload the board-specific sketch. The admin page is embedded in the firmware.
5. On first boot, connect through AP mode or configured Wi-Fi and open the web console and status dashboard.

## Architecture at a Glance

RS485 RX data is collected by the UART driver as frames and delivered through queues. The queued data fans out to the TCP bridge, WebSocket console, packet replay buffer, and baud scanner scoring module. Configuration is stored with NVS Preferences, while the status LED and dashboard expose runtime state.

## Documentation

- [Architecture](./docs/ARCHITECTURE.en.md)
- [Getting started](./docs/getting-started.en.md)
- [Hardware](./docs/hardware.en.md)
- [Configuration](./docs/configuration.en.md)
- [Security](./docs/security.en.md)
- [Troubleshooting](./docs/troubleshooting.en.md)
- [Roadmap](./docs/roadmap.en.md)

## Contributing and Security

See [CONTRIBUTING.en.md](./CONTRIBUTING.en.md) for contribution guidelines. Report security issues privately to `jaemyeong@me.com` or through GitHub Private Vulnerability Reporting.

## License

MIT License. Copyright (c) 2026 Jaemyeong Jin.
