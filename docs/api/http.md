# HTTP API
> 🌐 Read this in: **한국어** · [English](./http.en.md)

## PRD 매핑

- §5.1 Wi-Fi Provisioning
- §5.7 RS485 TX
- §5.14 Baud Scanner
- §5.16 OTA Update
- §5.17 Status Dashboard
- §5.19 Factory Reset

## 엔드포인트 초안

| Method | Path | 용도 | 인증 |
|---|---|---|---|
| GET | `/api/status` | RSSI, uptime, heap, queue, overflow | TODO |
| GET | `/api/config` | 현재 설정 조회 | TODO |
| POST | `/api/config` | 설정 저장 | TODO |
| POST | `/api/tx` | HEX 프레임 송신 | 필요 |
| GET | `/api/scanner/result` | Baud Scanner 결과 조회 | TODO |
| POST | `/api/scanner/start` | Baud Scanner 시작 | 필요 |
| POST | `/api/scanner/stop` | Baud Scanner 중지 | 필요 |
| POST | `/api/factory-reset` | NVS clear 후 재부팅 | 필요 |
| GET/POST | `/update` | ElegantOTA 엔드포인트 | 필요 |

## 요청/응답 스키마 placeholder

```json
{
  "ok": true,
  "data": {},
  "error": null
}
```

## 오류 코드 초안

| code | 설명 |
|---|---|
| `bad_hex` | HEX 문자열 검증 실패 |
| `auth_required` | 인증 필요 |
| `queue_full` | TX queue 가득 참 |
| `scanner_busy` | Scanner 실행 중 충돌 |
| `invalid_config` | 설정 검증 실패 |

## TODO

- 모든 endpoint의 request/response schema 확정
- Basic Auth 적용 범위 반영
- OTA 경로를 ElegantOTA 실제 경로와 동기화
