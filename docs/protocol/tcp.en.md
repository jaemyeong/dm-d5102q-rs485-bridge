# TCP Bridge Protocol
> 🌐 Read this in: [한국어](./tcp.md) · **English**

## PRD Mapping

- §5.9 TCP Bridge
- §5.10 TCP reconnect

## Mode Comparison

| Mode | Description | Benefit | TODO |
|---|---|---|---|
| Server | Device listens as a TCP server | Easy local diagnostics clients | Default candidate |
| Client | Device connects to a remote host | Easy logger/broker forwarding | Requires host setting |

It is still undecided whether both modes can run simultaneously or only one mode is active at a time.

## Broadcast Policy

- RS485 RX frames are broadcast to connected TCP clients.
- TX frames received from TCP are HEX-validated and forwarded to the RS485 TX queue.
- The target is 1-4 simultaneous clients.

## TX Arbitration

| Source | Priority | Notes |
|---|---|---|
| Web Console | TODO | Manual frame send |
| HTTP API | TODO | Automation tools |
| TCP Client | TODO | Bridge input |
| Packet Replay | TODO | Replay task |

## Reconnect Policy

- Client mode performs automatic reconnect.
- Keepalive is enabled.
- Backoff shape, fixed or exponential, will be decided during implementation.

## TODO

- Confirm default TCP port: 9876 is recommended.
- Decide per-client TX authorization policy.
- Decide wire format: binary stream or line-delimited HEX.
