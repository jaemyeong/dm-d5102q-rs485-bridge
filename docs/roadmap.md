# 로드맵
> 🌐 Read this in: **한국어** · [English](./roadmap.en.md)

## 0.1.0 목표

- 저장소와 문서 골격
- 보드별 펌웨어 디렉터리 분리
- `shared/` 단방향 동기화 정책
- RS485/TCP/WebSocket/API 문서 골격

## 이후 확장 후보

| 항목 | 설명 | PRD |
|---|---|---|
| MQTT | RX/TX 이벤트를 MQTT topic으로 발행 | §12 |
| Home Assistant | 센서/스위치 엔티티 연동 | §12 |
| Packet Decoder | DM-D5102Q 프레임 의미 해석 | §12 |
| Syslog | 원격 로그 전송 | §12 |
| Packet Save | 캡처를 파일로 저장 | §12 |
| mDNS | `.local` 이름으로 접근 | §12 |

## 우선순위 원칙

1. 안전한 패시브 모니터링을 먼저 구현합니다.
2. TX 기능은 인증과 UI 확인을 갖춘 뒤 확장합니다.
3. 프로토콜 해석은 충분한 사용자 소유 장비 캡처가 쌓인 뒤 진행합니다.

## TODO

- 0.1.0 milestone issue 작성
- 0.2.x 이후 기능 범위 확정
