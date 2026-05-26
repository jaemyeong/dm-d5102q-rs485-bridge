# 시작하기
> 🌐 Read this in: **한국어** · [English](./getting-started.en.md)

## 준비물

- M5Stack AtomS3-Lite 또는 Atom-Lite
- Atom RS485 Base
- USB-C 케이블과 안정적인 5V 전원
- Arduino IDE 2.x
- 사용자 본인 소유 DM-D5102Q 월패드 또는 분리된 RS485 테스트 환경

## Arduino IDE 설정

1. Arduino IDE 2.x를 설치합니다.
2. 보드 매니저 URL에 `https://espressif.github.io/arduino-esp32/package_esp32_index.json`을 추가합니다.
3. ESP32 보드 패키지를 설치합니다.
4. 대상 보드를 선택합니다.
   - AtomS3-Lite: M5Stack AtomS3 계열 보드
   - Atom-Lite: M5Stack Atom 계열 보드

## 라이브러리

라이브러리 매니저 또는 ZIP 설치로 아래 라이브러리를 준비합니다.

| 라이브러리 | 용도 |
|---|---|
| `M5Atom` | Atom-Lite 보드 지원 |
| `M5AtomS3` | AtomS3-Lite 보드 지원 |
| `WiFi` | ESP32 Wi-Fi |
| `ESPAsyncWebServer` | HTTP/WebSocket 서버 |
| `AsyncTCP` | Async TCP 기반 |
| `Preferences` | NVS 설정 저장 |
| `ElegantOTA` | OTA 업로드 |
| `ArduinoJson` | API JSON 직렬화 |

## 첫 빌드 흐름

1. `firmware/<board>/secrets.h.example`을 `secrets.h`로 복사합니다. 이 파일은 후속 구현 단계에서 추가됩니다.
2. `firmware/<board>/data/`를 LittleFS uploader로 업로드합니다.
3. 보드별 스케치를 컴파일하고 업로드합니다.
4. 첫 부팅에서 저장된 Wi-Fi가 없으면 AP 모드로 진입합니다.
5. 웹 브라우저에서 설정 폼, Console, Dashboard를 확인합니다.

## 첫 로그인

0.1.9부터 모든 웹 접근은 HTTP Basic Auth로 보호됩니다.

1. AP 모드에서 `http://192.168.4.1/`에 접속하면 브라우저가 즉시 인증 팝업을 표시합니다(`realm: "DM-D5102Q Bridge"`).
2. 기본 자격증명은 사용자 이름 `admin`, 비밀번호 `admin`입니다. `secrets.h`에서 `BRIDGE_DEFAULT_WEB_USER` / `BRIDGE_DEFAULT_WEB_PASSWORD`를 정의한 경우 그 값을 사용합니다.
3. 로그인 후 어드민 UI > 설정 > 보안 섹션에서 비밀번호를 즉시 변경하세요.
4. STA 모드로 전환되면 새 IP에서 다시 인증을 요구합니다.

## 안전 메모

- RS485 A/B/GND 배선을 연결하기 전에 월패드 전원과 공통 접지를 확인합니다.
- HEX TX 기능은 실제 버스에 프레임을 송신하므로 사용자가 소유한 장비에서만 사용합니다.
- 인터넷에 직접 노출하지 않습니다.

## TODO

- 정확한 보드 FQBN과 LittleFS uploader 절차 스크린샷 추가
- 첫 부팅 AP SSID/비밀번호 기본값 확정
