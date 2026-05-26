# WebSocket 프로토콜
> 🌐 Read this in: **한국어** · [English](./websocket.en.md)

## PRD 매핑

- §5.11 Web Console
- §5.12 WebSocket
- §5.17 Status Dashboard
- §5.18 Overflow Monitoring

## 엔드포인트

| 경로 | 용도 |
|---|---|
| `/ws/console` | RX/TX/status/error/system 이벤트 스트림 |

## 메시지 스키마 초안

```json
{
  "type": "rx",
  "ts": 123456789,
  "dir": "rx",
  "hex": "AA 55 01 02",
  "len": 4
}
```

## Batching 정책

- RX 이벤트는 20-100ms 간격으로 묶어 전송합니다.
- 브라우저 Web Console은 단일 클라이언트 사용을 권장합니다.
- Dashboard 상태 이벤트는 RX 스트림과 별도 type으로 전송합니다.

## 메시지 타입

| type | 방향 | 설명 |
|---|---|---|
| `rx` | device -> client | RS485 수신 프레임 |
| `tx` | 양방향 | 송신 요청 또는 송신 기록 |
| `status` | device -> client | RSSI, heap, queue 등 |
| `error` | device -> client | 검증 실패, overflow |
| `system` | device -> client | boot, reset, OTA 상태 |

## TODO

- batch payload를 배열로 보낼지 이벤트별로 보낼지 결정
- 인증 실패와 reconnect 메시지 포맷 확정
- Console pause 상태에서 서버 buffer 정책 확정
