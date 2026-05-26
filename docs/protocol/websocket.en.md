# WebSocket Protocol
> 🌐 Read this in: [한국어](./websocket.md) · **English**

## PRD Mapping

- §5.11 Web Console
- §5.12 WebSocket
- §5.17 Status Dashboard
- §5.18 Overflow Monitoring

## Endpoint

| Path | Purpose |
|---|---|
| `/ws/console` | Event stream for RX/TX/status/error/system |

## Draft Message Schema

```json
{
  "type": "rx",
  "ts": 123456789,
  "dir": "rx",
  "hex": "AA 55 01 02",
  "len": 4
}
```

## Batching Policy

- RX events are batched every 20-100ms.
- A single browser Web Console client is recommended.
- Dashboard status events use a separate type from RX stream data.

## Message Types

| type | Direction | Description |
|---|---|---|
| `rx` | device -> client | RS485 received frame |
| `tx` | bidirectional | TX request or TX log |
| `status` | device -> client | RSSI, heap, queue values |
| `error` | device -> client | Validation failure or overflow |
| `system` | device -> client | Boot, reset, OTA state |

## TODO

- Decide whether batched payloads are arrays or individual events.
- Finalize auth failure and reconnect message formats.
- Decide server buffering while Console pause is active.
