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

## TODO

- Finalize request/response schemas for every endpoint.
- Align OTA paths with actual ElegantOTA behavior.
