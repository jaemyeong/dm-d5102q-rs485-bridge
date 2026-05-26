# Status LED
> 🌐 Read this in: [한국어](./status-led.md) · **English**

## PRD Mapping

- §5.15 LED status indicator
- §5.17 Status dashboard
- §5.19 Factory reset

## Draft State Patterns

| State | Pattern | Notes |
|---|---|---|
| Booting | White pulse | Boot in progress |
| AP | Blue slow blink | Setup AP |
| Connecting | Yellow blink | STA connecting |
| Connected | Green solid | Wi-Fi connected |
| TCP | Cyan pulse | TCP client connected |
| RX | Short green flash | RX frame |
| TX | Short amber flash | TX frame |
| Error | Red blink | Error |
| Reset | Red fast blink | BtnA reset pending |
| OTA | Purple progress blink | Upload in progress |

## Brightness

| Setting | Default | Range |
|---|---:|---:|
| `led.brightness` | 32 | 0-255 |

## Priority

Long-running states such as error, factory reset, and OTA take priority over RX/TX flashes. The detailed priority order will be finalized during implementation.

## TODO

- Measure actual LED colors and board brightness differences.
- Decide RX/TX flash duration.
- Review alternative patterns for color accessibility.
