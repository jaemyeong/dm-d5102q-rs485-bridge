# 설정
> 🌐 Read this in: **한국어** · [English](./configuration.en.md)

## PRD 매핑

- Wi-Fi Provisioning: §5.1-5.2
- UART/Baud 설정: §5.3-5.4
- TCP Bridge: §5.9-5.10
- Status LED/Dashboard: §5.15, §5.17-5.18
- NVS 저장: §6

## NVS 네임스페이스 초안

| 네임스페이스 | 키 | 타입 | 기본값 | 검증 규칙 |
|---|---|---|---|---|
| `wifi` | `ssid` | string | empty | 1-32 bytes TODO |
| `wifi` | `psk` | string | empty | 8-63 bytes 또는 open TODO |
| `tcp` | `mode` | enum | `server` | `server`, `client` TODO |
| `tcp` | `port` | uint16 | `9876` | 1-65535 |
| `tcp` | `host` | string | empty | client 모드에서 필요 |
| `uart` | `baud` | uint32 | `9600` | 300-921600 |
| `uart` | `data_bits` | uint8 | `8` | 7 또는 8 TODO |
| `uart` | `stop_bits` | enum | `1` | 1 또는 2 |
| `uart` | `parity` | enum | `none` | none/even/odd |
| `led` | `brightness` | uint8 | `32` | 0-255 |
| `replay` | `count` | uint16 | TBD | RAM 예산 기반 |

## 설정 원칙

- 설정 변경은 검증 후 NVS에 저장합니다.
- 위험 작업(HEX TX, OTA, Factory Reset)은 인증 정책과 함께 검토합니다.
- 기본값은 실측 전까지 보수적으로 둡니다.

## TODO

- NVS 키 이름을 코드 구현 전 최종 확정
- Web UI 폼 필드와 API 스키마 동기화
- 기본 RS485 baud와 TCP 포트 사용자 확인
