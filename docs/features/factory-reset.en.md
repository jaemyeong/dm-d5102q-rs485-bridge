# Factory Reset
> 🌐 Read this in: [한국어](./factory-reset.md) · **English**

## PRD Mapping

- §5.19 Factory reset
- §6 NVS storage
- §5.1 Wi-Fi provisioning

## Conditions

| Input | Behavior |
|---|---|
| Hold BtnA for 5 seconds | Clear NVS and reboot |
| HTTP API request | Authenticate, then clear NVS TODO |
| Specific boot-time condition | Decide if needed TODO |

## Flow

1. Measure BtnA hold duration.
2. Show the Reset LED pattern as the hold approaches 5 seconds.
3. Clear NVS namespaces when the condition is met.
4. Reboot.
5. Enter AP mode on the next boot.

## Preserve/Delete Scope

| Data | Handling |
|---|---|
| Wi-Fi credentials | Delete |
| TCP settings | Delete |
| UART settings | Restore defaults |
| Basic Auth | Restore defaults TODO |
| Packet Replay | RAM only, cleared by reboot |

## TODO

- Decide whether to provide an HTTP factory reset endpoint.
- Confirm BtnA pins by measurement and update board settings.
- Finalize reset confirmation UX.
