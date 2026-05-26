# Packet Replay
> 🌐 Read this in: **한국어** · [English](./packet-replay.en.md)

## PRD 매핑

- §5.13 Packet Replay
- §5.7 RS485 TX

## 저장 방식

- RAM only buffer를 기본으로 합니다.
- 전원 재부팅 후 replay 데이터는 유지하지 않습니다.
- 패킷 수와 패킷당 최대 길이는 RAM 예산에 따라 결정합니다.

## 설정 후보

| 설정 | 설명 | 기본값 |
|---|---|---|
| `replay.count` | 저장할 최근 packet 수 | TODO |
| `replay.max_len` | packet당 최대 byte | TODO |
| `replay.include_tx` | TX 기록 포함 여부 | TODO |

## 재생 API

| Method | Path | 설명 |
|---|---|---|
| GET | `/api/replay` | replay buffer 목록 조회 TODO |
| POST | `/api/replay/send` | 선택 packet 재송신 TODO |
| DELETE | `/api/replay` | buffer clear TODO |

## 안전 고려

- Replay는 실제 RS485 버스에 프레임을 다시 송신할 수 있습니다.
- 인증과 UI 확인을 적용합니다.
- Scanner 실행 중 replay 요청 처리 정책이 필요합니다.

## TODO

- 기본 buffer count와 max packet size 확정
- replay 항목 메타데이터 필드 확정
- TX queue와 replay 간 우선순위 확정
