# shared
> 🌐 Read this in: [한국어](./README.md) · **English**

`shared/` is the master tree for source files and Web UI assets shared by the AtomS3-Lite and Atom-Lite firmware builds.

## Sync Direction

```text
shared/data/* -> firmware/atoms3-lite/data/*
shared/data/* -> firmware/atom-lite/data/*
shared/src/*  -> firmware/atoms3-lite/src/*
shared/src/*  -> firmware/atom-lite/src/*
```

Synchronization will be handled by `tools/sync_shared.sh`, added in a later implementation phase. Copying is one-way; board firmware `src/` and `data/` directories are synchronized output.

## Files Not Copied

- `<board>.ino`
- `board_config.h`
- `secrets.h.example`
- `partitions.csv`

## Forbidden Rules

- Do not add board-specific compile branches such as `#if defined(ARDUINO_M5STACK_ATOMS3)` under `shared/src/*`.
- Do not edit `firmware/*/src/*` directly.
- Keep board-specific pins, flash size, and LED pins only in each board's `board_config.h`.

## Module Responsibilities

| Directory | Responsibility |
|---|---|
| `src/app/` | BootSequence, FactoryReset, TaskDispatch |
| `src/network/` | Wi-Fi, provisioning, TCP bridge, reconnect |
| `src/rs485/` | Driver, PacketFramer, HexCodec, queues, replay |
| `src/scanner/` | BaudScanner, BaudScore |
| `src/web/` | WebServer, WebSocketHub, OTA, BasicAuth |
| `src/storage/` | Settings, NVS stores, DeviceConfig |
| `src/status/` | StatusLed, DeviceStatus, Metrics |
| `src/utils/` | Logger, HexFormat, TimeUtil, Version |

## TODO

- Implement `tools/sync_shared.sh`.
- Add drift checks after synchronization.
