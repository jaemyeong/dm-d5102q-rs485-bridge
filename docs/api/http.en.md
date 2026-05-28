# HTTP API
> 🌐 Read this in: [한국어](./http.md) · **English**

## PRD Mapping

- §5.1 Wi-Fi provisioning
- §5.7 RS485 TX
- §5.14 Baud scanner
- §5.16 OTA update
- §5.17 Status dashboard
- §5.19 Factory reset

## Draft Endpoints

> Every endpoint requires Basic Auth. Unauthenticated requests are rejected with [401 Unauthorized](#401-unauthorized).

| Method | Path | Purpose | Auth Required |
|---|---|---|---|
| GET | `/api/status` | RSSI, uptime, heap, queue, overflow | ✓ |
| GET | `/api/info` | Returns static metadata (firmware, board, TCP limits, queue capacity) | ✓ |
| POST | `/api/reboot` | Body `{}`. 200 → reboot 500 ms later. 409 if already scheduled | ✓ |
| GET | `/api/config` | Read current settings | ✓ |
| POST | `/api/config` | Save settings | ✓ |
| GET | `/api/wifi/scan` | List nearby Wi-Fi scan results | ✓ |
| POST | `/api/tx` | Send HEX frame | ✓ |
| GET | `/api/scanner/result` | Read baud scanner results | ✓ |
| POST | `/api/scanner/start` | Start baud scanner | ✓ |
| POST | `/api/scanner/stop` | Stop baud scanner | ✓ |
| POST | `/api/factory-reset/settings` | Reset settings only, then reboot | ✓ |
| POST | `/api/factory-reset/all` | Wipe the entire NVS, then reboot | ✓ |
| POST | `/update` | ElegantOTA firmware upload | ✓ |

## Request/Response Schema Placeholder

```json
{
  "ok": true,
  "data": {},
  "error": null
}
```

## Common Errors

### 401 Unauthorized

Returned when the request lacks valid Basic Auth credentials.

Response header: `WWW-Authenticate: Basic realm="DM-D5102Q Bridge"`

The body matches the ESPAsyncWebServer default message (`Authentication required`).

## Draft Error Codes

| code | Description |
|---|---|
| `bad_hex` | HEX string validation failed |
| `auth_required` | Authentication required |
| `queue_full` | TX queue is full |
| `scanner_busy` | Scanner is already running |
| `invalid_config` | Settings validation failed |

## GET /api/info

Auth required. Returns static metadata about the firmware build and configuration.

Example response:
```json
{
  "ok": true,
  "data": {
    "firmware": {
      "version": "0.1.10",
      "commit": "abc1234",
      "built_at": "2026-05-29T03:00:00Z",
      "board": "ESP32_DEV"
    },
    "tcp":   { "max_clients": 4 },
    "queue": { "capacity": 256 }
  }
}
```

## POST /api/reboot

Auth required. Send an empty JSON body (`{}`) to reboot the device approximately 500 ms later.

Success response:
```json
{ "queued": true, "scheduledMs": 500 }
```

409 when a reboot is already scheduled:
```json
{ "ok": false, "error": "reboot_in_progress" }
```

## GET /api/status — additional fields (0.1.10)

Added to `data.metrics`:
- `auth_failures` (number) — cumulative 401 auth failures since boot.
- `last_auth_fail_ago_ms` (number) — milliseconds since last 401 (or `-1` if none).

Added to `data.device`:
- `commit` (string) — short build git SHA.
- `built_at` (string) — build ISO-8601 UTC timestamp.

## TODO

- Finalize request/response schemas for every endpoint.
- Align OTA paths with actual ElegantOTA behavior.
