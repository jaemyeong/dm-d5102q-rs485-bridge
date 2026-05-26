# Status LED
> 🌐 Read this in: **한국어** · [English](./status-led.en.md)

## PRD 매핑

- §5.15 LED Status Indicator
- §5.17 Status Dashboard
- §5.19 Factory Reset

## 상태 패턴 초안

| 상태 | 패턴 | 메모 |
|---|---|---|
| Booting | 흰색 pulse | 부팅 중 |
| AP | 파란색 slow blink | 설정 AP |
| Connecting | 노란색 blink | STA 연결 중 |
| Connected | 녹색 solid | Wi-Fi 연결 |
| TCP | 청록색 pulse | TCP client 연결 |
| RX | 녹색 짧은 flash | RX frame |
| TX | 주황색 짧은 flash | TX frame |
| Error | 빨간색 blink | 오류 |
| Reset | 빨간색 fast blink | BtnA reset pending |
| OTA | 보라색 progress blink | 업로드 중 |

## 밝기

| 설정 | 기본값 | 범위 |
|---|---:|---:|
| `led.brightness` | 32 | 0-255 |

## 우선순위

오류, Factory Reset, OTA 같은 장기 상태가 RX/TX flash보다 우선합니다. 세부 우선순위는 구현 단계에서 확정합니다.

## TODO

- 실제 LED 색상과 보드별 밝기 차이 측정
- RX/TX flash duration 확정
- 색각 접근성 대체 패턴 검토
