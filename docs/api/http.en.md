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

| Method | Path | Purpose | Auth |
|---|---|---|---|
| GET | `/api/status` | RSSI, uptime, heap, queue, overflow | TODO |
| GET | `/api/config` | Read current settings | TODO |
| POST | `/api/config` | Save settings | TODO |
| POST | `/api/tx` | Send HEX frame | Required |
| GET | `/api/scanner/result` | Read baud scanner results | TODO |
| POST | `/api/scanner/start` | Start baud scanner | Required |
| POST | `/api/scanner/stop` | Stop baud scanner | Required |
| POST | `/api/factory-reset` | Clear NVS and reboot | Required |
| GET/POST | `/update` | ElegantOTA endpoints | Required |

## Request/Response Schema Placeholder

```json
{
  "ok": true,
  "data": {},
  "error": null
}
```

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
- Reflect the chosen Basic Auth scope.
- Align OTA paths with actual ElegantOTA behavior.
