# OTA Update
> 🌐 Read this in: [한국어](./ota.md) · **English**

## PRD Mapping

- §5.16 OTA update
- §9 AP password + Basic Auth

## Integration Direction

Use ElegantOTA to support `.bin` firmware uploads. OTA is a local maintenance feature for the diagnostics device and is not designed for direct internet exposure.

## Upload Flow

1. The user opens the authenticated OTA page.
2. The user selects the `.bin` file for the correct board.
3. The status LED switches to the OTA pattern during upload.
4. The device reboots after upload completes.
5. The dashboard shows the new version.

## Validation Items

| Item | Description |
|---|---|
| File extension | Allow only `.bin` TODO |
| Partition size | Confirm it fits the OTA slot |
| Board compatibility | Prevent AtomS3-Lite/Atom-Lite mismatch TODO |
| Authentication | Apply Basic Auth |

## TODO

- Confirm actual ElegantOTA endpoint paths.
- Decide board-specific firmware artifact naming rules.
- Decide rollback messages and LED pattern on failure.
