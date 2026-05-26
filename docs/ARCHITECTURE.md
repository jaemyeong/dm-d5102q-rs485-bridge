# 아키텍처
> 🌐 Read this in: **한국어** · [English](./ARCHITECTURE.en.md)

## 목표

DM-D5102Q RS485 TCP Bridge는 사용자 본인 소유 IoT 장비의 RS485 트래픽을 관찰하고, 필요한 경우 수동 HEX 프레임을 송신하며, 웹 콘솔과 TCP 브리지로 진단 데이터를 노출하는 도구입니다.

## 런타임 구성

| 영역 | 책임 | PRD |
|---|---|---|
| Core0 | 네트워크, HTTP, WebSocket, TCP Bridge, OTA | §5.1-5.2, §5.9-5.12, §5.16 |
| Core1 | RS485 UART RX/TX, 프레이밍, 큐, Baud Scanner | §5.3-5.8, §5.13-5.14 |
| 공통 | 설정 저장, 상태 LED, Metrics, Factory Reset | §5.15, §5.17-5.19, §6, §8 |

## 데이터 흐름

```text
RS485 Bus
  -> Rs485Driver
  -> PacketFramer
  -> RxQueue
  -> TCP Bridge
  -> WebSocket Console
  -> Packet Replay Buffer
  -> Baud Scanner Score
```

TX 경로는 Web Console, HTTP API, TCP Bridge에서 들어온 HEX 프레임을 검증한 뒤 `TxQueue`에 넣고, RS485 드라이버가 DE/RE 제어와 UART flush를 수행합니다.

## 모듈 인터페이스

| 모듈 | 입력 | 출력 | 비고 |
|---|---|---|---|
| `app/` | 부팅 이벤트, 버튼 상태 | 태스크 시작, Factory Reset | 보드별 설정은 `board_config.h`에 격리 |
| `network/` | NVS Wi-Fi/TCP 설정 | Wi-Fi 상태, TCP 세션 | Client/Server 모드 정책 TODO |
| `rs485/` | UART 바이트, TX 요청 | 프레임, 카운터, overflow | idle-gap/delimiter/length 전략 TODO |
| `scanner/` | baud 범위, 샘플 수 | 품질 점수 | Console RX 제한 정책 TODO |
| `web/` | HTTP/WS 요청 | UI, JSON, OTA | Basic Auth 범위 TODO |
| `storage/` | 설정 변경 | Preferences NVS | 키 검증 규칙 문서화 필요 |
| `status/` | 상태 이벤트, metrics | LED 패턴, dashboard 값 | 10개 상태 패턴 |
| `utils/` | 공통 변환 | 로그, HEX format, 시간 | 보드 분기 금지 |

## 설계 원칙

- 보드별 차이는 `firmware/<board>/board_config.h`에만 둡니다.
- `shared/src/*`에는 보드별 `#ifdef`를 넣지 않습니다.
- `shared/`를 마스터로 두고 `firmware/<board>/src/*`는 동기화 산출물로 취급합니다.
- 네트워크 장애와 RS485 overflow는 metrics에 누적해 Dashboard에서 확인할 수 있게 합니다.

## TODO

- TCP Bridge 동시 운영 여부 확정
- Web UI LittleFS/PROGMEM 호스팅 방식 확정
- Packet Replay 기본 버퍼 크기 확정
