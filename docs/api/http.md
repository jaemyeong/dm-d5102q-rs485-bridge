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

> 모든 엔드포인트는 Basic Auth 인증을 요구합니다. 인증되지 않은 요청은 [401 Unauthorized](#401-unauthorized)로 응답됩니다.

| Method | Path | 용도 | 인증 필요 |
|---|---|---|---|
| GET | `/api/status` | RSSI, uptime, heap, queue, overflow | ✓ |
| GET | `/api/config` | 현재 설정 조회 | ✓ |
| POST | `/api/config` | 설정 저장 | ✓ |
| GET | `/api/wifi/scan` | 주변 Wi-Fi 스캔 결과 조회 | ✓ |
| POST | `/api/tx` | HEX 프레임 송신 | ✓ |
| GET | `/api/scanner/result` | Baud Scanner 결과 조회 | ✓ |
| POST | `/api/scanner/start` | Baud Scanner 시작 | ✓ |
| POST | `/api/scanner/stop` | Baud Scanner 중지 | ✓ |
| POST | `/api/factory-reset/settings` | 설정만 초기화 후 재부팅 | ✓ |
| POST | `/api/factory-reset/all` | NVS 전체 wipe 후 재부팅 | ✓ |
| POST | `/update` | ElegantOTA 펌웨어 업로드 | ✓ |

## 요청/응답 스키마 placeholder

```json
{
  "ok": true,
  "data": {},
  "error": null
}
```

## 공통 에러

### 401 Unauthorized

인증 헤더가 없거나 잘못된 경우 반환됩니다.

응답 헤더: `WWW-Authenticate: Basic realm="DM-D5102Q Bridge"`

응답 본문 형식은 ESPAsyncWebServer 기본값(`Authentication required`)을 따릅니다.

## 오류 코드 초안

| code | 설명 |
|---|---|
| `bad_hex` | HEX 문자열 검증 실패 |
| `auth_required` | 인증 자격 증명 필요 |
| `queue_full` | TX queue 가득 참 |
| `scanner_busy` | Scanner 실행 중 충돌 |
| `invalid_config` | 설정 검증 실패 |

## TODO

- 모든 endpoint의 request/response schema 확정
- OTA 경로를 ElegantOTA 실제 경로와 동기화
