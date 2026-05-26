# WebSocket API
> 🌐 Read this in: **한국어** · [English](./websocket.en.md)

## 연결

| 항목 | 값 |
|---|---|
| Endpoint | `/ws/console` |
| Protocol | WebSocket over HTTP |
| 권장 클라이언트 | 단일 Web Console client |

## 클라이언트 -> 장치

```json
{
  "type": "tx",
  "hex": "AA 55 01 02"
}
```

| type | 설명 | TODO |
|---|---|---|
| `tx` | HEX 프레임 송신 요청 | 권한 확인 |
| `subscribe` | stream/filter 설정 | 필요 여부 결정 |
| `ping` | 연결 유지 확인 | 서버 응답 포맷 결정 |

## 장치 -> 클라이언트

```json
{
  "type": "status",
  "ts": 123456789,
  "rssi": -61,
  "heap_free": 123456,
  "rx_count": 42,
  "tx_count": 3
}
```

| type | 설명 |
|---|---|
| `rx` | RS485 RX 프레임 |
| `tx` | RS485 TX 기록 |
| `status` | Dashboard 상태 |
| `error` | 입력 검증 또는 런타임 오류 |
| `system` | boot, OTA, reset 등 시스템 이벤트 |

## TODO

- `subscribe` 메시지 도입 여부 결정
- status payload 필드와 Dashboard 표시 항목 동기화
- 오류 메시지 다국어 처리 방식 결정
