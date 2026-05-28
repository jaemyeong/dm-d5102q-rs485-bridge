# Changelog
> 🌐 Read this in: [한국어](./CHANGELOG.md) · **English**

This project follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and uses [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [0.1.10] - 2026-05-29
### Added
- Web admin header redesign: 46px topbar + click-to-expand detail drawer.
- Always-visible health summary (`OK / WARN / FAIL / OFFLINE`) and active alert count.
- One-click **Reboot** action with confirmation dialog and auto-reconnect polling.
- `GET /api/info` exposes firmware version, commit, build time, board, TCP max clients, queue capacity.
- `POST /api/reboot` — auth-protected reboot endpoint (200 → `ESP.restart()` after 500 ms; 409 if already scheduled).

### Changed
- `GET /api/status` `metrics` now includes `auth_failures` and `last_auth_fail_ago_ms`.
- `data.device` exposes `commit` and `built_at`.
- Firmware bumped to 0.1.10.

### Security
- Authentication failures are tracked in firmware and surfaced as a header alert.

## [0.1.9] - 2026-05-27

### Changed
- Web admin and provisioning screens now require HTTP Basic Auth on every route (static HTML, `/api/*`, `/update` OTA, `/ws/console` WebSocket).
- Removed the `basic_auth` toggle field from the configuration schema and the admin UI — authentication is always enforced.

### Security
- `/update` OTA upload endpoint is now authenticated; previously accessible without credentials.
- `/ws/console` WebSocket handshake is now authenticated.
- All GET endpoints (`/api/status`, `/api/config`, `/api/wifi/scan`, `/api/scanner/result`) now require credentials.

## [0.1.2] - 2026-05-26

### Added

- Embedded gzip admin UI in firmware for single firmware-bin OTA distribution
- Detected Wi-Fi network list and SSID selection on the settings page
- Automatic page reload scheduling after successful OTA upload

### Changed

- OTA upload now flashes only one firmware image

## [0.1.1] - 2026-05-26

### Added

- Path-based admin routing and Korean admin UI
- Separate OTA handling for firmware images and LittleFS web UI images
- Reboot notice screen after provisioning save

- Repository configuration, license, and community documentation skeletons
- Arduino IDE 2.x firmware directory layout for AtomS3-Lite and Atom-Lite
- `shared/` master tree and one-way board synchronization policy
- RS485, TCP, WebSocket, and HTTP API documentation skeletons
- Feature documentation skeletons for Wi-Fi provisioning, baud scanner, packet replay, web console, OTA, status LED, dashboard, and factory reset
- PC tools, tests, captures, and hardware documentation scaffolding

### Changed

- Provisioning is shown only in AP/factory-reset state and removed from the normal admin menu
- Wi-Fi settings and factory reset action now live in the admin settings page

## [0.1.0] - TBD

Planned initial public release.
