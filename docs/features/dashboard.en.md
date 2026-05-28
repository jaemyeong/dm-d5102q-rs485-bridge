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

## Header Drawer

The 46px topbar exposes a persistent health summary on the right: `● OK` / `⚠ WARN` / `✕ FAIL` / `⊘ OFFLINE` chip, the active alert count (`⚠ N`), and the ⏻ reboot button.

- **Expand**: click the right cluster or press Enter / Space.
- **Dismiss**: click the cluster again, press Esc, click the ✕ button, or click outside the drawer.
- **State**: open across route navigation within a session; closed after page reload.

The drawer renders three panels (Network / Traffic / Device) and the active alert list. Alerts are derived from 12 sources (Wi-Fi loss, RS485 RX silence, queue overflow, auth failure, etc.).

## Reboot Button

Pressing the ⏻ button opens a confirmation modal. Clicking **Reboot** posts to `POST /api/reboot`; the device restarts ~500 ms later. The modal switches to `재부팅 중…`/`Rebooting…` and polls `/api/status` once per second; on a 200 response the page reloads automatically. After a 90 s timeout, a [Retry] option appears.
