# Architecture
> 🌐 Read this in: [한국어](./ARCHITECTURE.md) · **English**

## Goal

DM-D5102Q RS485 TCP Bridge is a diagnostics tool for user-owned IoT equipment. It observes RS485 traffic, optionally sends manual HEX frames, and exposes diagnostic data through a web console and TCP bridge.

## Runtime Layout

| Area | Responsibility | PRD |
|---|---|---|
| Core0 | Network, HTTP, WebSocket, TCP bridge, OTA | §5.1-5.2, §5.9-5.12, §5.16 |
| Core1 | RS485 UART RX/TX, framing, queues, baud scanner | §5.3-5.8, §5.13-5.14 |
| Shared | Settings, status LED, metrics, factory reset | §5.15, §5.17-5.19, §6, §8 |

## Data Flow

```text
RS485 Bus
  -> Rs485Driver
  -> PacketFramer
  -> RxQueue
  -> TCP Bridge
  -> WebSocket Console
  -> Packet Replay Buffer
  -> Baud Scanner Score
```

The TX path accepts HEX frames from the web console, HTTP API, and TCP bridge, validates them, queues them in `TxQueue`, and lets the RS485 driver handle DE/RE control plus UART flush.

## Module Interfaces

| Module | Input | Output | Notes |
|---|---|---|---|
| `app/` | Boot events, button state | Task startup, factory reset | Board settings stay in `board_config.h` |
| `network/` | NVS Wi-Fi/TCP settings | Wi-Fi state, TCP sessions | Client/server mode policy TODO |
| `rs485/` | UART bytes, TX requests | Frames, counters, overflow | idle-gap/delimiter/length strategy TODO |
| `scanner/` | Baud range, sample count | Quality score | Console RX restriction policy TODO |
| `web/` | HTTP/WS requests | UI, JSON, OTA | Basic Auth scope TODO |
| `storage/` | Settings updates | Preferences NVS | Key validation rules need implementation |
| `status/` | State events, metrics | LED patterns, dashboard values | 10 status patterns |
| `utils/` | Shared conversions | Logs, HEX format, time | Board branching is forbidden |

## Design Rules

- Board differences belong only in `firmware/<board>/board_config.h`.
- Do not add board-specific `#ifdef` branches under `shared/src/*`.
- Treat `shared/` as the master tree and `firmware/<board>/src/*` as synchronized output.
- Network failures and RS485 overflow are accumulated in metrics for dashboard visibility.

## TODO

- Decide whether TCP Bridge modes can run simultaneously.
- Decide between LittleFS and PROGMEM Web UI hosting.
- Decide the default Packet Replay buffer size.
