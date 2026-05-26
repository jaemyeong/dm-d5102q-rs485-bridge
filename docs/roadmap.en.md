# Roadmap
> 🌐 Read this in: [한국어](./roadmap.md) · **English**

## 0.1.0 Goals

- Repository and documentation scaffolding
- Separate board firmware directories
- One-way `shared/` synchronization policy
- RS485/TCP/WebSocket/API documentation skeletons

## Future Extension Candidates

| Item | Description | PRD |
|---|---|---|
| MQTT | Publish RX/TX events to MQTT topics | §12 |
| Home Assistant | Sensor/switch entity integration | §12 |
| Packet Decoder | Interpret DM-D5102Q frame meaning | §12 |
| Syslog | Remote log forwarding | §12 |
| Packet Save | Store captures to files | §12 |
| mDNS | Access through a `.local` name | §12 |

## Prioritization Rules

1. Implement safe passive monitoring first.
2. Expand TX features only after authentication and UI confirmation exist.
3. Start protocol decoding after enough captures from user-owned equipment are available.

## TODO

- Create 0.1.0 milestone issues.
- Decide 0.2.x feature scope.
