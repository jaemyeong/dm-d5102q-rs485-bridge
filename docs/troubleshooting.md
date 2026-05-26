# 문제 해결
> 🌐 Read this in: **한국어** · [English](./troubleshooting.en.md)

## 자주 만나는 증상

| 증상 | 가능한 원인 | 확인 |
|---|---|---|
| AP가 보이지 않음 | 부팅 실패, 전원 부족, LED 초기화 중 | USB 전원, 시리얼 로그 |
| Wi-Fi 연결 실패 | SSID/PSK 오류, RSSI 낮음 | AP fallback, NVS reset |
| RS485 RX 없음 | A/B 반전, GND 미연결, baud 불일치 | 배선, Baud Scanner |
| 깨진 HEX | baud/parity 불일치, 노이즈 | UART 설정, 종단저항 |
| TCP 접속 끊김 | keepalive/backoff 설정, 네트워크 불안정 | Dashboard counters |
| OTA 실패 | 파티션 크기 부족, 인증 실패 | partitioning 문서, 로그 |
| WebSocket 지연 | batch interval, 브라우저 단일 클라이언트 제한 | Console pause/filter |

## 복구 절차

1. USB 시리얼 로그를 확인합니다.
2. BtnA 5초 Factory Reset으로 NVS를 초기화합니다.
3. AP 모드에서 Wi-Fi와 UART 기본값을 다시 설정합니다.
4. RS485 배선을 분리한 상태에서 자체 Console과 Dashboard를 확인합니다.
5. 배선을 다시 연결하고 RX 품질을 확인합니다.

## 로그 수집

- 펌웨어 버전, 보드명, 부팅 시간, RSSI를 함께 기록합니다.
- HEX 캡처는 민감한 주소나 식별값이 있을 수 있으므로 공유 전 익명화합니다.
- 재현 절차에는 RX/TX 방향과 baud 설정을 포함합니다.

## TODO

- 실제 에러 코드와 LED 패턴 매핑 추가
- 시리얼 로그 예시 추가
