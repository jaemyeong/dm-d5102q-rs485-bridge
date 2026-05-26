# Status Dashboard
> 🌐 Read this in: **한국어** · [English](./dashboard.en.md)

## PRD 매핑

- §5.17 Status Dashboard
- §5.18 Overflow Monitoring
- §5.9 TCP Bridge
- §5.15 LED Status Indicator

## 표시 항목

| 항목 | 설명 |
|---|---|
| RSSI | Wi-Fi 신호 세기 |
| IP | STA/AP IP 주소 |
| Uptime | 부팅 후 시간 |
| Heap | free/min heap |
| RX/TX count | 프레임 카운터 |
| TCP clients | 연결 수 |
| Queue usage | RX/TX queue 사용량 |
| Overflow | UART/queue/dropped 카운터 |
| Baud | 현재 UART baud |
| Mode | AP/STA, TCP client/server |

## 갱신 주기

- WebSocket status 이벤트를 기본으로 사용합니다.
- 기본 갱신 주기는 1초 후보입니다.
- Console RX batching과 Dashboard status interval은 분리합니다.

## 오류 표시

Overflow가 증가하면 Dashboard에 last error와 카운터를 표시합니다. 리셋 버튼은 후속 구현에서 제공 여부를 결정합니다.

## TODO

- status JSON schema 확정
- queue usage 단위와 임계값 확정
- Dashboard warning 색상과 threshold 확정
