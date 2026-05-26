# WebSocket API
> 🌐 Read this in: [한국어](./websocket.md) · **English**

## Connection

| Item | Value |
|---|---|
| Endpoint | `/ws/console` |
| Protocol | WebSocket over HTTP |
| Recommended client | Single Web Console client |

## Client -> Device

```json
{
  "type": "tx",
  "hex": "AA 55 01 02"
}
```

| type | Description | TODO |
|---|---|---|
| `tx` | Request HEX frame send | Check authorization |
| `subscribe` | Stream/filter settings | Decide if needed |
| `ping` | Keep connection alive | Decide server response format |

## Device -> Client

```json
{
  "type": "status",
  "ts": 123456789,
  "rssi": -61,
  "heap_free": 123456,
  "rx_count": 42,
  "tx_count": 3
}
```

| type | Description |
|---|---|
| `rx` | RS485 RX frame |
| `tx` | RS485 TX log |
| `status` | Dashboard state |
| `error` | Input validation or runtime error |
| `system` | System events such as boot, OTA, reset |

## TODO

- Decide whether to introduce `subscribe` messages.
- Align status payload fields with dashboard display fields.
- Decide how localized error messages are handled.
