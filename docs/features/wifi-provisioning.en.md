# Wi-Fi Provisioning
> 🌐 Read this in: [한국어](./wifi-provisioning.md) · **English**

## PRD Mapping

- §5.1 Wi-Fi provisioning
- §5.2 Wi-Fi connection management
- §6 NVS storage

## AP Entry Conditions

| Condition | Behavior |
|---|---|
| No saved SSID | Start AP mode |
| Wi-Fi retries exceeded | AP fallback |
| After factory reset | Clear NVS and enter AP mode |
| User-requested setup | TODO |

## Web Form Fields

| Field | Description | Validation |
|---|---|---|
| SSID | Wi-Fi network name | 1-32 bytes TODO |
| PSK | Wi-Fi password | 8-63 bytes or open TODO |
| Hostname | Device name | Extend when mDNS is added |
| Basic Auth user | Web UI auth | Scope TODO |
| Basic Auth password | Web UI auth | Minimum length TODO |

## Fallback Policy

- Retry count and backoff will be decided during implementation.
- AP mode still shows the connection failure reason on the dashboard.
- It is undecided whether AP remains active after STA connects successfully.

## TODO

- Decide default AP SSID/password.
- Decide whether STA+AP can run simultaneously.
- Decide whether to show SSID scan results before saving.
