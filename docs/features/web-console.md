# Web Console
> 🌐 Read this in: **한국어** · [English](./web-console.en.md)

## PRD 매핑

- §5.11 Web Console
- §5.12 WebSocket
- §5.5 RS485 RX 수신
- §5.7 RS485 TX

## 표시 항목

| 항목 | 설명 |
|---|---|
| Timestamp | 수신/송신 시각 |
| Direction | RX 또는 TX |
| Length | byte 길이 |
| HEX | 공백 구분 HEX dump |
| Status | overflow, validation error 등 |

## 필터

| 필터 | 설명 |
|---|---|
| RX/TX | 방향별 표시 |
| HEX contains | 특정 byte sequence 포함 여부 |
| length | 길이 범위 |
| error only | 오류 이벤트만 표시 |

## 하이라이트 색상 초안

| 이벤트 | 색상 |
|---|---|
| RX | blue/green 계열 TODO |
| TX | amber 계열 TODO |
| Error | red 계열 TODO |
| System | neutral 계열 TODO |

## 조작

- Pause/Resume
- Clear
- Manual HEX send
- Copy selected frame
- Export는 후속 확장 후보

## TODO

- Console buffer line 수 확정
- pause 중 서버/브라우저 buffer 정책 확정
- UI 색상 토큰 확정
