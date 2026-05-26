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

## Basic Auth Scope

This is still undecided.

| Option | Description | Notes |
|---|---|---|
| All pages | Protect every HTTP/WS entry point | Simple and safer default |
| Risky actions only | Protect OTA, scanner, HEX TX, and similar actions | Better usability, higher miss risk |

## Internet Exposure Recommendations

- Do not use direct port forwarding.
- Use VPN or SSH tunneling if remote access is needed.
- Treat WebSocket and TCP bridge traffic as plaintext.

## TODO

- Decide Basic Auth scope.
- Decide whether HEX TX needs separate authorization.
- Decide AP password default and strength rules.
