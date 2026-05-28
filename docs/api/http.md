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
| GET | `/api/info` | 펌웨어/보드/TCP 한도/큐 용량 등 정적 메타 반환 | ✓ |
| POST | `/api/reboot` | 본문 `{}`. 200 → 500ms 후 재부팅. 이미 예약돼 있으면 409 | ✓ |
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

## GET /api/info

인증 필요. 펌웨어 빌드와 설정의 정적 메타데이터를 반환합니다.

응답 예시:
```json
{
  "ok": true,
  "data": {
    "firmware": {
      "version": "0.1.10",
      "commit": "abc1234",
      "built_at": "2026-05-29T03:00:00Z",
      "board": "ESP32_DEV"
    },
    "tcp":   { "max_clients": 4 },
    "queue": { "capacity": 256 }
  }
}
```

## POST /api/reboot

인증 필요. 본문에 빈 JSON 객체(`{}`)를 보내면 약 500ms 후 장치가 재시작합니다.

성공 응답:
```json
{ "queued": true, "scheduledMs": 500 }
```

이미 재부팅이 예약돼 있는 경우 409:
```json
{ "ok": false, "error": "reboot_in_progress" }
```

## GET /api/status — 추가 필드 (0.1.10)

`data.metrics`에 추가된 필드:
- `auth_failures` (number) — 부팅 후 누적 401 인증 실패 횟수.
- `last_auth_fail_ago_ms` (number) — 마지막 401 발생으로부터 경과한 밀리초 (없으면 `-1`).

`data.device`에 추가된 필드:
- `commit` (string) — 빌드 git 짧은 SHA.
- `built_at` (string) — 빌드 ISO-8601 UTC 시각.

## TODO

- 모든 endpoint의 request/response schema 확정
- OTA 경로를 ElegantOTA 실제 경로와 동기화
