# Configuration
> 🌐 Read this in: [한국어](./configuration.md) · **English**

## PRD Mapping

- Wi-Fi provisioning: §5.1-5.2
- UART/Baud settings: §5.3-5.4
- TCP bridge: §5.9-5.10
- Status LED/Dashboard: §5.15, §5.17-5.18
- NVS storage: §6

## Draft NVS Namespaces

| Namespace | Key | Type | Default | Validation |
|---|---|---|---|---|
| `wifi` | `ssid` | string | empty | 1-32 bytes TODO |
| `wifi` | `psk` | string | empty | 8-63 bytes or open TODO |
| `tcp` | `mode` | enum | `server` | `server`, `client` TODO |
| `tcp` | `port` | uint16 | `9876` | 1-65535 |
| `tcp` | `host` | string | empty | Required in client mode |
| `uart` | `baud` | uint32 | `9600` | 300-921600 |
| `uart` | `data_bits` | uint8 | `8` | 7 or 8 TODO |
| `uart` | `stop_bits` | enum | `1` | 1 or 2 |
| `uart` | `parity` | enum | `none` | none/even/odd |
| `web` | `basic_auth` | bool | `true` | Scope TODO |
| `led` | `brightness` | uint8 | `32` | 0-255 |
| `replay` | `count` | uint16 | TBD | Based on RAM budget |

## Configuration Rules

- Validate settings before writing them to NVS.
- Review risky operations such as HEX TX, OTA, and factory reset together with authentication policy.
- Keep defaults conservative until hardware measurements are confirmed.

## TODO

- Finalize NVS key names before implementation.
- Keep Web UI form fields and API schemas synchronized.
- Confirm default RS485 baud and TCP port with the user.
