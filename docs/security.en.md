# Security Design
> 🌐 Read this in: [한국어](./security.md) · **English**

## Threat Model

This device is an RS485 debugging and self-diagnostics tool used on a local network or temporary AP. The main risk is unauthorized access to the Web UI, WebSocket, TCP bridge, OTA, or HEX TX features.

## Protected Assets

| Asset | Risk | Direction |
|---|---|---|
| Wi-Fi credentials | NVS disclosure | Avoid logs, mask settings UI |
| HEX TX | Unintended frame send | Authentication, UI confirmation, audit log TODO |
| OTA | Malicious firmware upload | Basic Auth, local network only |
| WebSocket | Plaintext traffic exposure | Trusted network, no internet exposure |
| TCP Bridge | Misuse by external clients | Port limits, client limits |

## AP Password Policy

- AP mode is used for initial setup and Wi-Fi fallback.
- The default AP password will be decided during implementation.
- Open AP mode should be limited to development builds if allowed at all.

## Web Authentication Policy

Starting with 0.1.9, every web access requires HTTP Basic Auth. The gate covers:

- Static admin UI (`/`, `/dashboard`, `/console`, `/ota`, `/about`, and any other undefined GET path)
- All `/api/*` endpoints (read and write alike)
- OTA firmware upload (`/update`)
- WebSocket (`/ws/console`) handshake

Unauthenticated requests receive `401 Unauthorized` with the header `WWW-Authenticate: Basic realm="DM-D5102Q Bridge"`.

Default credentials resolve in this order: value stored in NVS → `BRIDGE_DEFAULT_WEB_USER` / `BRIDGE_DEFAULT_WEB_PASSWORD` macros in `secrets.h` → compiled defaults `admin` / `admin`. Change the password immediately after first boot.

If credentials are lost, hold the boot button for 5 seconds to wipe settings only (WiFi credentials preserved) or 8+ seconds for a full NVS wipe.

## Internet Exposure Recommendations

- Do not use direct port forwarding.
- Use VPN or SSH tunneling if remote access is needed.
- Treat WebSocket and TCP bridge traffic as plaintext.

## TODO

- Decide whether HEX TX needs separate authorization.
- Decide AP password default and strength rules.
