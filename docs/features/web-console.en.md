# Web Console
> 🌐 Read this in: [한국어](./web-console.md) · **English**

## PRD Mapping

- §5.11 Web Console
- §5.12 WebSocket
- §5.5 RS485 RX
- §5.7 RS485 TX

## Display Fields

| Field | Description |
|---|---|
| Timestamp | RX/TX time |
| Direction | RX or TX |
| Length | Byte length |
| HEX | Space-separated HEX dump |
| Status | Overflow, validation error, and similar states |

## Filters

| Filter | Description |
|---|---|
| RX/TX | Filter by direction |
| HEX contains | Match a byte sequence |
| length | Length range |
| error only | Show only error events |

## Draft Highlight Colors

| Event | Color |
|---|---|
| RX | blue/green family TODO |
| TX | amber family TODO |
| Error | red family TODO |
| System | neutral family TODO |

## Controls

- Pause/Resume
- Clear
- Manual HEX send
- Copy selected frame
- Export is a future extension candidate

## TODO

- Decide console buffer line count.
- Decide server/browser buffering while paused.
- Finalize UI color tokens.
