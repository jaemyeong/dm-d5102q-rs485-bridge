# 파티션 구성
> 🌐 Read this in: **한국어** · [English](./partitioning.en.md)

## PRD 매핑

- OTA Update: §5.16
- NVS 저장: §6
- AtomS3-Lite 8MB / Atom-Lite 4MB 차이: §3.3 참고

## 목표

OTA를 위해 `app0`/`app1` 이중 앱 파티션을 사용하고, 웹 UI 파일은 LittleFS 영역에 둡니다. 실제 `partitions.csv`는 후속 구현 단계에서 추가합니다.

## 예정 구조

| 파티션 | 용도 | 비고 |
|---|---|---|
| `nvs` | Wi-Fi, TCP, UART, UI 설정 | Preferences 사용 |
| `otadata` | OTA 부트 슬롯 상태 | ESP32 OTA 기본 |
| `app0` | 실행 앱 슬롯 A | OTA 크기 산정 필요 |
| `app1` | 실행 앱 슬롯 B | OTA 크기 산정 필요 |
| `spiffs` 또는 `littlefs` | Web UI 정적 파일 | 이름은 코어 지원 확인 후 확정 |

## 8MB와 4MB 고려

| 항목 | AtomS3-Lite 8MB | Atom-Lite 4MB |
|---|---|---|
| OTA 여유 | 상대적으로 넉넉함 | 앱 크기 제한 엄격 |
| LittleFS 크기 | UI 확장 가능 | 최소 UI 권장 |
| 로그 저장 | 향후 검토 | 기본은 RAM/외부 캡처 권장 |

## TODO

- ElegantOTA와 ESP32 Arduino core 조합에서 안정적인 앱 슬롯 크기 확정
- LittleFS 파티션 이름과 uploader 호환성 확인
- 실제 `partitions.csv` 작성 시 보드별 파일 분리 여부 결정
