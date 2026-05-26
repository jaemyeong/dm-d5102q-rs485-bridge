# 하드웨어
> 🌐 Read this in: **한국어** · [English](./hardware.en.md)

## 지원 조합

| 보드 | RS485 베이스 | 플래시 | 상태 |
|---|---|---:|---|
| AtomS3-Lite | Atom RS485 Base | 8MB 추정 | 1차 대상 |
| Atom-Lite | Atom RS485 Base | 4MB 추정 | 1차 대상 |

## Atom RS485 Base 연결

| 신호 | 설명 | 메모 |
|---|---|---|
| A | RS485 differential A | 월패드 단자 실측 필요 |
| B | RS485 differential B | A/B 반전 시 RX 품질 저하 |
| GND | 공통 접지 | 장비 보호를 위해 필수 |
| 5V | 베이스 전원 | USB 전원과 충돌 여부 확인 |

## 보드별 핀 차이

| 매크로 | AtomS3-Lite | Atom-Lite | 상태 |
|---|---|---|---|
| `RS485_UART_NUM` | 2 | 2 | 추정 |
| `RS485_RX_PIN` | G5 | G22 | 실측 필요 |
| `RS485_TX_PIN` | G6 | G19 | 실측 필요 |
| `RS485_DE_PIN` | G7 | G23 | 실측 필요 |
| `STATUS_LED_PIN` | G35 | G27 | 실측 필요 |
| `FACTORY_RESET_BTN_PIN` | G41 | G39 | 실측 필요 |

## 종단저항

- 기존 월패드 RS485 버스의 종단 상태를 먼저 확인합니다.
- 디버깅 장비를 병렬로 붙이는 경우 중복 종단저항이 통신 품질을 낮출 수 있습니다.
- 장거리 케이블 또는 독립 테스트 버스에서는 120Ω 종단을 검토합니다.

## 전원과 케이스

- USB 5V 전원 사용을 기본으로 가정합니다.
- 월패드 단자대에서 전원을 가져오는 구성은 실측 후 문서화합니다.
- 케이스는 RS485 단자와 USB 포트가 노출되고 BtnA 접근이 가능해야 합니다.

## TODO

- Atom RS485 Base 리비전별 핀맵 확인
- 실측 사진과 배선도 추가
