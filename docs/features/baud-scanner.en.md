# Baud Scanner
> 🌐 Read this in: [한국어](./baud-scanner.md) · **English**

## PRD Mapping

- §5.4 Baud rate policy
- §5.14 Baud scanner

## Pages

| Path | Description |
|---|---|
| `/scanner` | Baud sweep and quality scoring UI |
| `/api/scanner/start` | Start scanner |
| `/api/scanner/result` | Read results |

## Input Fields

| Field | Description | Default |
|---|---|---|
| start | Start baud | TODO |
| end | End baud | TODO |
| step | Increment or preset | TODO |
| sample | Collection time/frame count per baud | TODO |
| parity | Parity candidate | current setting |

## Flow

1. Save the current UART settings.
2. Apply the chosen Console RX restriction policy.
3. Score frame quality during the sample window for each baud.
4. Record score, frame count, printable/HEX stability, and overflow.
5. Restore the original UART settings.

## Result Table

| baud | frames | bytes | score | overflow | note |
|---:|---:|---:|---:|---:|---|
| TODO | TODO | TODO | TODO | TODO | TODO |

## TODO

- Decide whether Console RX is forced off or user-configurable during scanning.
- Finalize the quality scoring formula.
- Finalize preset list and custom range UI.
