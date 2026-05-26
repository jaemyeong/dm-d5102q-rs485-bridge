# shared
> 🌐 Read this in: **한국어** · [English](./README.en.md)

`shared/`는 AtomS3-Lite와 Atom-Lite 펌웨어가 함께 사용하는 공통 소스와 Web UI 자산의 마스터 트리입니다.

## 동기화 방향

```text
shared/data/* -> firmware/atoms3-lite/data/*
shared/data/* -> firmware/atom-lite/data/*
shared/src/*  -> firmware/atoms3-lite/src/*
shared/src/*  -> firmware/atom-lite/src/*
```

동기화는 후속 구현 단계에서 추가될 `tools/sync_shared.sh`가 수행합니다. 복사는 단방향이며, 보드별 펌웨어 디렉터리의 `src/`와 `data/`는 동기화 산출물로 취급합니다.

## 복사하지 않는 파일

- `<board>.ino`
- `board_config.h`
- `secrets.h.example`
- `partitions.csv`

## 금지 규칙

- `shared/src/*` 안에 보드별 `#if defined(ARDUINO_M5STACK_ATOMS3)` 같은 컴파일 분기를 넣지 않습니다.
- `firmware/*/src/*`를 직접 편집하지 않습니다.
- 보드 핀, 플래시 크기, LED 핀 같은 차이는 보드별 `board_config.h`에만 둡니다.

## 모듈 책임

| 디렉터리 | 책임 |
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

- `tools/sync_shared.sh` 구현
- 동기화 후 drift check 추가
