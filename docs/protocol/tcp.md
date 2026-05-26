# TCP Bridge 프로토콜
> 🌐 Read this in: **한국어** · [English](./tcp.en.md)

## PRD 매핑

- §5.9 TCP Bridge
- §5.10 TCP Reconnect

## 모드 비교

| 모드 | 설명 | 장점 | TODO |
|---|---|---|---|
| Server | 장치가 TCP 서버로 listen | 로컬 진단 클라이언트 연결 쉬움 | 기본 후보 |
| Client | 장치가 원격 호스트로 접속 | 로거/브로커로 전송 쉬움 | host 설정 필요 |

동시 운영 여부와 모드 전환식 운영 여부는 아직 미확정입니다.

## Broadcast 정책

- RS485 RX 프레임은 연결된 TCP 클라이언트에 broadcast합니다.
- TCP에서 들어온 TX 프레임은 HEX 검증 후 RS485 TX queue로 전달합니다.
- 1-4 동시 클라이언트를 목표로 합니다.

## TX Arbitration

| 입력원 | 우선순위 | 메모 |
|---|---|---|
| Web Console | TODO | 수동 명령 송신 |
| HTTP API | TODO | 자동화 도구 |
| TCP Client | TODO | Bridge 입력 |
| Packet Replay | TODO | 재생 작업 |

## 재연결 정책

- Client 모드는 auto reconnect를 수행합니다.
- keepalive를 활성화합니다.
- backoff는 고정/지수 중 후속 구현에서 결정합니다.

## TODO

- 기본 TCP 포트 확정: 9876 추천
- 클라이언트별 TX 권한 정책 확정
- binary stream과 line-delimited HEX 중 wire format 확정
