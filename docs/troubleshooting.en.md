# Troubleshooting
> 🌐 Read this in: [한국어](./troubleshooting.md) · **English**

## Common Symptoms

| Symptom | Possible Cause | Check |
|---|---|---|
| AP is not visible | Boot failure, weak power, LED init phase | USB power, serial logs |
| Wi-Fi fails | Wrong SSID/PSK, low RSSI | AP fallback, NVS reset |
| No RS485 RX | Reversed A/B, missing GND, baud mismatch | Wiring, baud scanner |
| Corrupt HEX | Baud/parity mismatch, noise | UART settings, termination |
| TCP disconnects | keepalive/backoff settings, unstable network | Dashboard counters |
| OTA fails | Partition too small, auth failure | Partitioning doc, logs |
| WebSocket lag | Batch interval, single-client browser limit | Console pause/filter |

## Recovery Flow

1. Check USB serial logs.
2. Hold BtnA for 5 seconds to factory-reset NVS.
3. Reconfigure Wi-Fi and UART defaults in AP mode.
4. Disconnect RS485 wiring and verify the local console/dashboard.
5. Reconnect wiring and check RX quality.

## Log Collection

- Record firmware version, board name, boot time, and RSSI together.
- HEX captures may contain sensitive addresses or identifiers; anonymize before sharing.
- Include RX/TX direction and baud settings in reproduction steps.

## TODO

- Add actual error code and LED pattern mapping.
- Add serial log examples.
