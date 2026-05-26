# DM-D5102Q RS485 TCP Bridge
> 🌐 Read this in: **한국어** · [English](./README.en.md)

M5Stack AtomS3-Lite / Atom-Lite와 Atom RS485 베이스를 사용해 DM-D5102Q 월패드 RS485 통신을 관찰하고 점검하는, 사용자 본인 소유 IoT 장비의 디버깅·자가 진단 도구입니다. 패킷 캡처, HEX 프레임 전송, WebSocket 콘솔, TCP 브리지, OTA 업데이트 같은 기능을 단계적으로 제공합니다.

## 상태

- 현재 버전: 0.1.9 (Unreleased)
- 빌드 시스템: Arduino IDE 2.x
- 대상 보드: M5Stack AtomS3-Lite, M5Stack Atom-Lite
- 펌웨어 구조: `firmware/atoms3-lite/`, `firmware/atom-lite/` 분리
- 공통 코드: `shared/`를 마스터로 두고 보드별 펌웨어로 단방향 동기화

## 기능 범위

| PRD | 기능 |
|---|---|
| §5.1-5.2 | Wi-Fi Provisioning, 연결 재시도, AP fallback |
| §5.3-5.8 | RS485 UART 설정, RX/TX, 프레이밍, 반이중 제어 |
| §5.9-5.10 | TCP Bridge Client/Server, 재연결, keepalive, backoff |
| §5.11-5.12 | Web Console, WebSocket batching |
| §5.13-5.14 | Packet Replay, Baud Scanner |
| §5.15-5.19 | LED 상태, OTA, Dashboard, Overflow, Factory Reset |

## 빠른 시작

1. Arduino IDE 2.x와 ESP32 보드 패키지를 설치합니다.
2. 권장 라이브러리(`M5Atom`, `M5AtomS3`, `ESPAsyncWebServer`, `AsyncTCP`, `Preferences`, `ElegantOTA`, `ArduinoJson`)를 설치합니다.
3. 대상 보드 디렉터리에서 `secrets.h.example`을 `secrets.h`로 복사하고 Wi-Fi/AP 값을 설정합니다. 이 파일은 후속 구현 단계에서 추가됩니다.
4. 보드별 스케치를 컴파일하고 업로드합니다. 관리자 페이지는 펌웨어에 내장됩니다.
5. 첫 부팅 시 AP 모드 또는 설정된 Wi-Fi로 접속해 웹 콘솔과 상태 대시보드를 확인합니다.

## 아키텍처 한눈에

RS485 RX는 UART 드라이버에서 프레임 단위로 수집되어 큐로 전달됩니다. 큐의 데이터는 TCP Bridge, WebSocket Console, Packet Replay, Baud Scanner 평가 모듈로 팬아웃됩니다. 설정은 NVS Preferences에 저장하고, 상태 LED와 Dashboard가 런타임 상태를 표시합니다.

## 문서

- [아키텍처](./docs/ARCHITECTURE.md)
- [시작하기](./docs/getting-started.md)
- [하드웨어](./docs/hardware.md)
- [설정](./docs/configuration.md)
- [보안](./docs/security.md)
- [문제 해결](./docs/troubleshooting.md)
- [로드맵](./docs/roadmap.md)

## 기여와 보안

기여 방법은 [CONTRIBUTING.md](./CONTRIBUTING.md)를 참고하세요. 보안 취약점은 공개 이슈 대신 `jaemyeong@me.com` 또는 GitHub Private Vulnerability Reporting으로 신고해 주세요.

## 라이센스

MIT License. Copyright (c) 2026 Jaemyeong Jin.
