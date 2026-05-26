# Contributing Guide
> 🌐 Read this in: [한국어](./CONTRIBUTING.md) · **English**

## Development Environment

- Arduino IDE 2.x
- ESP32 board manager URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- Recommended libraries: `M5Atom`, `M5AtomS3`, `ESPAsyncWebServer` (ESP32Async fork), `AsyncTCP`, `Preferences`, `ElegantOTA`, and `ArduinoJson`
- LittleFS uploader plugin for validating each board's `data/` upload path

## Branch Strategy

- Features: `feat/<topic>`
- Fixes: `fix/<topic>`
- Documentation: `docs/<topic>`
- Maintenance: `chore/<topic>`

## Commit Rules

Use Conventional Commits 1.0.0.

Examples:

```text
feat: add packet framing strategy
fix: handle tcp reconnect backoff
docs: update baud scanner guide
```

## shared Synchronization Rules

`shared/` is the master tree. `shared/data/*` and `shared/src/*` are copied one-way into `firmware/atoms3-lite/` and `firmware/atom-lite/` through `tools/sync_shared.sh`.

Do not:

- add board-specific `#ifdef` branches under `shared/src/*`
- edit `firmware/*/src/*` directly
- copy `<board>.ino`, `board_config.h`, `secrets.h.example`, or `partitions.csv` automatically

## Code Style

- C++/Arduino: 4-space indent
- Web/YAML/JSON: 2-space indent
- Python tools/tests: PEP 8, `ruff`, and `black --check`
- Markdown: Korean is the primary documentation language; update matching `.en.md` files together

## PR Checklist

- [ ] Compile both boards: AtomS3-Lite and Atom-Lite
- [ ] Run shared synchronization when `shared/` changes
- [ ] Update related Korean and English documentation pairs
- [ ] Confirm the change stays within user-owned equipment diagnostics
- [ ] Document security impact according to `SECURITY.md`

## Issue Reporting

Use the GitHub issue templates for bugs, feature requests, hardware questions, and PRD change proposals. Do not report vulnerabilities in public issues; email `jaemyeong@me.com` instead.
