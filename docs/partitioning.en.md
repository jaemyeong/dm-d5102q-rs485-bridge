# Partitioning
> 🌐 Read this in: [한국어](./partitioning.md) · **English**

## PRD Mapping

- OTA update: §5.16
- NVS storage: §6
- AtomS3-Lite 8MB / Atom-Lite 4MB difference: see §3.3

## Goal

Use dual `app0`/`app1` app partitions for OTA and keep Web UI files in a LittleFS region. The actual `partitions.csv` files will be added in a later implementation phase.

## Planned Layout

| Partition | Purpose | Notes |
|---|---|---|
| `nvs` | Wi-Fi, TCP, UART, UI settings | Uses Preferences |
| `otadata` | OTA boot slot state | ESP32 OTA default |
| `app0` | App slot A | Size calculation needed |
| `app1` | App slot B | Size calculation needed |
| `spiffs` or `littlefs` | Static Web UI files | Name depends on core support |

## 8MB and 4MB Considerations

| Item | AtomS3-Lite 8MB | Atom-Lite 4MB |
|---|---|---|
| OTA headroom | Relatively comfortable | App size limits are stricter |
| LittleFS size | Allows a larger UI | Minimal UI recommended |
| Log storage | Future option | Prefer RAM/external capture by default |

## TODO

- Decide stable app slot sizes for ElegantOTA with the ESP32 Arduino core.
- Confirm LittleFS partition naming and uploader compatibility.
- Decide whether actual `partitions.csv` files are board-specific.
