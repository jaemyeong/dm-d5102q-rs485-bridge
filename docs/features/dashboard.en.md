# Status Dashboard
> 🌐 Read this in: [한국어](./dashboard.md) · **English**

## PRD Mapping

- §5.17 Status dashboard
- §5.18 Overflow monitoring
- §5.9 TCP bridge
- §5.15 LED status indicator

## Display Fields

| Field | Description |
|---|---|
| RSSI | Wi-Fi signal strength |
| IP | STA/AP IP address |
| Uptime | Time since boot |
| Heap | free/min heap |
| RX/TX count | Frame counters |
| TCP clients | Connected clients |
| Queue usage | RX/TX queue usage |
| Overflow | UART/queue/dropped counters |
| Baud | Current UART baud |
| Mode | AP/STA, TCP client/server |

## Refresh Interval

- WebSocket status events are the default transport.
- The default refresh interval candidate is 1 second.
- Console RX batching and dashboard status interval are separate settings.

## Error Display

When overflow increases, the dashboard shows the last error and counters. A reset button is undecided for later implementation.

## TODO

- Finalize status JSON schema.
- Decide queue usage units and thresholds.
- Decide dashboard warning colors and thresholds.
