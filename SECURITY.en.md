# Security Policy
> 🌐 Read this in: [한국어](./SECURITY.md) · **English**

## Reporting Channels

Please do not report vulnerabilities in public issues. Use one of these private channels instead.

- Email: `jaemyeong@me.com`
- GitHub Private Vulnerability Reporting

## Response SLA

- Acknowledgement: within 72 hours
- Initial assessment: within 7 days
- Patch or mitigation plan: within 30 days

## Scope

- Wi-Fi, TCP, WebSocket, and HTTP endpoints
- OTA upload paths and authentication flow
- Web UI and manual HEX frame send features
- NVS-stored settings and sensitive configuration handling
- Arduino/ESP32 build chain and release artifacts

## Out of Scope

- DM-D5102Q wall pad firmware or external services
- Upstream vulnerabilities in third-party libraries themselves
- User network configuration, power, and wiring issues

## Known Security Limits

- AP passwords and Basic Auth are minimum protection layers for a local diagnostics device.
- The default Web UI and WebSocket transport do not provide HTTPS/TLS.
- HEX TX can send frames onto a real RS485 bus and must be limited to authorized users.
- Do not expose the device directly to the internet; use a VPN or isolated management network when needed.

## Supported Versions

| Version | Status |
|---|---|
| 0.1.x | Early development, security fixes accepted |
