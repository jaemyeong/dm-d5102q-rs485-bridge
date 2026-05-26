# captures
> 🌐 Read this in: [한국어](./README.md) · **English**

This directory temporarily stores RS485 packet captures and analysis artifacts. Binary captures and raw logs are ignored by default through `.gitignore`.

## Recommended Metadata

| Item | Description |
|---|---|
| Date/time | Capture time and timezone |
| Board | AtomS3-Lite or Atom-Lite |
| Firmware version | `FW_VERSION` |
| UART settings | baud/data/stop/parity |
| Wiring | A/B/GND and termination status |
| Scenario | Boot, button, wall pad action, and similar context |

## Anonymization

- Remove household identifiers, addresses, and personally identifiable strings before sharing.
- Do not put a home address or real equipment location in capture file names.
- Public issues should include only the minimum HEX excerpt needed.

## TODO

- Add a standard capture metadata template.
- Finalize packet log extensions and retention policy.
