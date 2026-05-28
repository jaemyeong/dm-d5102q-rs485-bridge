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

## 헤더 드로우어

상단 46px 토픽바 우측에 `● 정상` / `⚠ 경고` / `✕ 장애` / `⊘ 오프라인` 헬스 칩, 활성 알림 카운트(`⚠ N`), 그리고 ⏻ 재부팅 버튼이 항상 표시됩니다.

- **펼치기**: 우측 영역 클릭 또는 키보드 Enter/Space
- **닫기**: 다시 클릭, Esc, drawer 우측 ✕, 또는 영역 밖 클릭
- **상태 유지**: 같은 세션 내 페이지 이동 시 열림 상태 유지 (새로고침은 닫힘)

드로우어는 3개 패널(Network / Traffic / Device)과 활성 알림 목록을 표시합니다. 알림은 12가지 source(Wi-Fi 끊김, RS485 RX 정적, 큐 오버플로우, 인증 실패 등)에서 자동 도출됩니다.

## 재부팅 버튼

헤더의 ⏻ 버튼을 누르면 확인 모달이 나타납니다. **재부팅**을 누르면 `POST /api/reboot`이 호출되고 500ms 후 장치가 재시작합니다. 모달은 자동으로 `재부팅 중…`으로 전환되며, 장치가 응답하면 페이지를 자동 새로고침합니다. 90초 내 응답이 없으면 [다시 시도] 옵션이 나타납니다.
