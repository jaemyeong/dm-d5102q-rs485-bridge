# Packet Replay
> 🌐 Read this in: [한국어](./packet-replay.md) · **English**

## PRD Mapping

- §5.13 Packet replay
- §5.7 RS485 TX

## Storage Model

- Use a RAM-only buffer by default.
- Replay data is not preserved across reboot.
- Packet count and maximum packet length depend on RAM budget.

## Candidate Settings

| Setting | Description | Default |
|---|---|---|
| `replay.count` | Number of recent packets to keep | TODO |
| `replay.max_len` | Maximum bytes per packet | TODO |
| `replay.include_tx` | Include TX records | TODO |

## Replay API

| Method | Path | Description |
|---|---|---|
| GET | `/api/replay` | List replay buffer TODO |
| POST | `/api/replay/send` | Re-send selected packet TODO |
| DELETE | `/api/replay` | Clear buffer TODO |

## Safety Notes

- Replay can send frames back onto a real RS485 bus.
- Authentication and UI confirmation are required.
- A policy is needed for replay requests while the scanner is running.

## TODO

- Decide default buffer count and max packet size.
- Finalize replay item metadata fields.
- Decide priority between TX queue and replay.
