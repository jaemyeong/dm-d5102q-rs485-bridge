# DM-D5102Q RS485–Wi‑Fi Bridge 클린룸 전체 설계서

문서 버전: 1.0  
작성 기준일: 2026-09-01 KST  
상태: 구현 착수 전 기준선  
대상 하드웨어: M5Stack Atom Lite(C008) + Atomic RS485 Base(A131)  
대상 저장소: dm-d5102q-rs485-bridge

## 1. 목적과 범위

이 문서는 DM-D5102Q 계열 RS485 버스를 로컬 Wi‑Fi 네트워크와 연결하는 전용 브리지의 독립 구현 계약서다. 펌웨어, 배선, 통신 경계, 웹 관리 화면, 보안, 복구, 시험, 출시까지 필요한 요구사항을 단계별 마일스톤으로 정의한다.

명시적 제외: Atom S3 Lite와 그 보드 지원 코드, 빌드 환경, 핀 정의, 테스트, 문서는 이 설계의 범위에 포함하지 않는다.

### 1.1 포함 기능

- RS485 바이트의 무손실 수신과 프레임 조립
- 수신 데이터를 raw TCP와 WebSocket으로 전달
- 로컬 웹 대시보드, 실시간 콘솔, 설정, 진단
- Wi‑Fi 초기 프로비저닝과 재연결
- 실제 버스 관측에 근거한 수동형 baud 탐색
- 물리 확인을 거친 제한적 RS485 송신
- 이중 OTA 파티션, 서명 검증, 실패 시 롤백
- 상태 LED, 버튼, 공장 초기화, 운영 로그
- 호스트 테스트, 하드웨어 통합 시험, 72시간 내구 시험

### 1.2 비목표

- 확인되지 않은 DM-D5102Q 상위 프로토콜의 의미 해석
- Modbus 호환을 가장하는 동작
- 클라우드, MQTT, 계정 서버, 원격 인터넷 관리
- 다중 보드 공통 추상화
- 버스 패킷의 영구 저장 또는 자동 재생
- 가짜 데이터, 합성 스캔 결과, 오프라인 데모 값을 실제 상태처럼 표시
- 자동 능동 탐색이나 승인 없는 송신

## 2. 클린룸 원칙

이 문서는 기존 구현의 파일 구조, 클래스, 함수, 상수 이름을 복제하기 위한 문서가 아니다. 공개 하드웨어 문서, 물리 측정, 외부에서 관측 가능한 입출력, 독립 시험 벡터만을 구현 근거로 삼는다.

### 2.1 역할 분리

- 증거 담당: 공식 문서, 배선 사진, 로직 분석기 캡처, 네트워크 캡처를 수집하고 출처·해시·시간을 기록한다.
- 명세 담당: 관측 결과를 요구사항과 시험 벡터로 바꾼다. 기존 소스 코드는 구현 지시로 인용하지 않는다.
- 구현 담당: 이 문서와 승인된 시험 벡터만 받는다. 기존 저장소의 레거시 구현은 보지 않는다.
- 검증 담당: 블랙박스 시험과 하드웨어 측정으로 요구사항 충족 여부를 판정한다.

### 2.2 출처 관리

각 요구사항과 시험 벡터에는 다음 중 하나를 붙인다.

- PUB: 제조사 또는 라이브러리의 공개 문서
- OBS: 실제 장치·버스·네트워크 관측
- DEC: 본 설계에서 내린 명시적 제품 결정
- UNV: 아직 검증되지 않은 가정

증거 원본은 읽기 전용으로 보존하고 SHA-256, 수집 시각, 장비, 펌웨어 버전, 배선 상태를 함께 기록한다.

### 2.3 엄격한 의미의 클린룸 주의

이 문서 작성 과정에서는 기존 프로젝트가 기능 표면과 피해야 할 결함을 확인하는 비규범 자료로 사용되었다. 따라서 법적 의미의 완전한 source-blind 클린룸 증명이 필요하면 M0에서 역할을 실제로 분리하고, 구현 담당자에게는 본 문서와 공개·관측 증거만 전달해야 한다.

## 3. 확정 사항과 미검증 게이트

### 3.1 확정 사항

- Atom Lite는 ESP32-PICO-D4, 520 KB SRAM, 4 MB flash, 2.4 GHz Wi‑Fi를 사용한다. PUB
- Atom Lite의 사용자 포트에는 GPIO26과 GPIO32가 노출된다. PUB
- Atomic RS485 Base는 SP3485EE 계열 변환기와 6–24 V 입력을 5 V로 낮추는 DC-DC를 포함한다. PUB
- Atom Lite 연결 후보는 GPIO26=UART TX, GPIO32=UART RX이며 별도 MCU DE 핀은 사용하지 않는다. PUB+DEC
- Atom Lite의 RGB LED는 GPIO27, 버튼은 GPIO39다. PUB
- Atomic RS485 Base에는 내장 120 Ω 종단저항이 없다. PUB
- RS485 송신은 기본 비활성화한다. DEC

### 3.2 구현 전 반드시 검증할 항목

- DM-D5102Q 현장 baud, data bits, parity, stop bits
- A/B 표기와 실제 극성
- 브리지가 버스 종단인지 중간 탭인지
- 버스 전원 전압, 접지 전위차, USB와 버스 전원의 동시 연결 안전성
- 선택한 Atomic RS485 Base 하드웨어 리비전의 자동 송수신 방향 전환
- 최대 실제 프레임 길이와 프레임 간 간격
- 기존 마스터가 있는 버스에서 허용되는 송신 타이밍과 충돌 규칙
- 현장 Wi‑Fi RSSI, 채널 혼잡, DHCP 및 격리 정책

검증 전 호환성 시작값은 3840 baud, 8 data bits, no parity, 1 stop bit, 20 ms inter-byte gap으로 둘 수 있지만 이는 장치 프로토콜 확인값이 아니다.

## 4. 시스템 개요

    DM-D5102Q RS485 Bus
             │ A/B/GND
             ▼
    Atomic RS485 Base
             │ UART RX/TX
             ▼
    ┌──────────────── Atom Lite 펌웨어 ────────────────┐
    │ UART 수신 → 프레임 조립 → RX 큐 → 팬아웃         │
    │                                  ├→ raw TCP      │
    │                                  ├→ WebSocket    │
    │                                  ├→ RAM 캡처     │
    │                                  └→ 상태 지표     │
    │                                                  │
    │ Web/TCP 입력 → 인증·정책 → TX 중재 → TX 큐 → UART│
    │                                                  │
    │ Wi‑Fi │ 설정 저장 │ OTA │ LED·버튼 │ 감사 로그   │
    └──────────────────────────────────────────────────┘
             │ 2.4 GHz Wi‑Fi
             ▼
       로컬 브라우저 / 승인된 TCP 클라이언트

### 4.1 설계 원칙

- 한 보드, 한 제품, 한 펌웨어 환경만 둔다.
- 정상 운전은 비차단 cooperative event loop로 구성한다.
- hot path에는 동적 메모리 할당을 두지 않는다.
- 모든 큐, 프레임, 요청 본문, 클라이언트 수에 상한을 둔다.
- 수신 경로는 송신·웹·로그 장애와 분리하고 가장 높은 우선순위를 갖는다.
- 설정 적용 결과는 live 적용과 재부팅 필요를 구분해 반환한다.
- 관측값과 추정값을 UI와 API에서 구분한다.
- 안전을 위해 기능보다 송신 차단과 복구 가능성을 우선한다.

## 5. 하드웨어 설계

### 5.1 필수 BOM

| 수량 | 품목 | 요구사항 |
|---:|---|---|
| 1 | M5Stack Atom Lite C008 | ESP32-PICO-D4, 4 MB flash |
| 1 | M5Stack Atomic RS485 Base A131 | SP3485EE, 6–24 V DC-DC |
| 1 | VH-3.96 4P 터미널 | 베이스와 기계적으로 호환 |
| 1 | 차폐 연선 | A/B 한 쌍, 필요 시 GND 포함 |
| 0 또는 1 | 120 Ω 저항 | 브리지가 물리적 버스 끝일 때만 |
| 1 | USB-C 데이터 케이블 | 플래시·시리얼 진단 |
| 1 | 절연 또는 전류 제한 전원 | 최초 시운전용 |
| 1 | USB 로직 분석기 또는 오실로스코프 | RX/TX와 A/B 검증 |

### 5.2 MCU 핀 계약

| 기능 | Atom Lite GPIO | 정책 |
|---|---:|---|
| RS485 UART TX | 26 | 하드웨어 리비전 검증 후 고정 |
| RS485 UART RX | 32 | 하드웨어 리비전 검증 후 고정 |
| 상태 RGB LED | 27 | 낮은 기본 밝기 |
| 사용자 버튼 | 39 | 입력 전용, active-low 확인 |
| RS485 DE/RE | 없음 | 베이스 자동 방향 전환을 실측한 뒤 확정 |

### 5.3 현장 배선 규칙

- 전원을 끈 상태에서 단자 라벨, 극성, 연속성을 먼저 확인한다.
- A와 B는 차동 연선으로 묶고 스타 배선을 만들지 않는다.
- GND 연결 여부는 양쪽 장비 설명서와 접지 전위 측정으로 결정한다.
- 120 Ω은 물리적 버스 양 끝에만 둔다. 중간 노드에는 추가하지 않는다.
- 버스 전원과 USB 전원을 동시에 연결하지 않는 것을 기본 시운전 정책으로 한다. 동시 연결은 회로도와 역급전 측정이 통과한 뒤 허용한다.
- 최초 연결은 TX가 소프트웨어와 정책 양쪽에서 비활성화된 수신 전용 펌웨어로 한다.
- A/B가 뒤바뀐 경우 자동으로 송신해 찾지 말고 수신 오류와 오실로스코프 파형으로 판정한다.

### 5.4 전기적 합격 기준

- 전원 인가 후 10분 동안 레귤레이터와 변환기 온도가 제조사 범위 안에 있다.
- idle 상태 A/B 차동과 공통 모드 전압이 트랜시버 허용 범위 안에 있다.
- 115200 baud 시험 패턴에서 UART RX 바이트 오류가 1시간 동안 0이다.
- 송신 종료 후 버스가 규정 시간 안에 release되고 기존 마스터 통신을 방해하지 않는다.
- USB 연결·해제와 버스 전원 재인가 시 재부팅 루프나 역급전이 없다.

## 6. 펌웨어 구성

### 6.1 구현 단위

| 단위 | 책임 | 금지 사항 |
|---|---|---|
| 보드 지원 | 핀, LED, 버튼, 시리얼 포트 | 다중 보드 분기 |
| 설정 저장 | 검증, 버전, 원자적 저장, 초기화 | 런타임 지표의 빈번한 flash 기록 |
| Wi‑Fi 관리 | STA, 제한적 AP, backoff, 상태 | 무기한 공개 AP |
| UART 수신 | 바이트 수집, 오류 계수 | 웹 처리 때문에 수신 정지 |
| 프레임 조립 | gap·delimiter·length 방식 | 확인되지 않은 의미 해석 |
| RX/TX 큐 | 고정 크기, overflow 계수 | 무제한 String 또는 heap 큐 |
| TCP 브리지 | raw byte 전달, client 상한 | 기본 송신 허용 |
| 웹 관리 | API, UI, WebSocket | 가짜 데이터 |
| 송신 중재 | arm, 만료, rate limit, 충돌 차단 | 입력 즉시 UART write |
| 실측 스캐너 | 수동 UART 후보 관측 | 합성 점수 |
| OTA | 서명, inactive slot, rollback | 인증 전 body 처리 |
| 상태·감사 | 지표, 오류 원인, 보안 이벤트 | 비밀번호·Wi‑Fi 키 로깅 |

### 6.2 런타임 상태

- BOOT: 설정 슬롯 검증, 원인 코드 기록, TX 강제 차단
- PROVISIONING: 물리 진입 또는 자격 증명 없음, 제한 시간 AP
- CONNECTING: STA 연결 시도, 지수 backoff
- RUN_RX_ONLY: 정상 기본 상태, 수신·관측만 허용
- RUN_TX_ARMED: 물리 확인 후 제한 시간·제한량 송신 허용
- DEGRADED: 큐 overflow, Wi‑Fi 단절, UART 오류 등
- UPDATING: 신규 요청과 TX 차단, OTA만 수행
- REBOOT_PENDING: 응답 flush 후 재부팅
- RESET_PENDING: LED 경고와 취소 창 뒤 초기화

모든 부팅은 TX 차단 상태에서 시작한다. arm 상태는 저장하지 않으며 Wi‑Fi 단절, 재부팅, OTA, 설정 변경, 만료 시 즉시 해제한다.

### 6.3 실행 예산

- 정상 event loop 1회 p99: 5 ms 이하
- UART RX 처리: loop마다 가용 바이트를 상한 내에서 우선 소진
- 최대 프레임: 초기 256 bytes, M1 실측으로만 상향
- UART RX ring: 2048 bytes
- RX 큐: 32 frames
- TX 큐: 8 frames
- RAM 캡처 ring: 64 frames
- raw TCP client: 최대 2
- WebSocket client: 최대 2
- 일반 JSON body: 최대 4096 bytes
- flash app image: 각 1.5 MiB OTA slot 기준 1.25 MiB 이하
- steady-state free heap: 80 KiB 이상
- 부하 중 free heap: 50 KiB 이상
- largest free block: 32 KiB 이상

상한은 측정 없이 늘리지 않는다. 상한 초과는 조용히 자르지 않고 원인별 drop counter와 이벤트를 남긴다.

## 7. RS485 데이터 평면

### 7.1 수신

1. UART 이벤트 또는 polling으로 바이트와 하드웨어 오류를 읽는다.
2. 바이트마다 monotonic timestamp를 기록한다.
3. 선택한 framing 방식으로 프레임을 완성한다.
4. 고정 RX 큐에 넣는다.
5. 한 번의 fan-out에서 TCP, WebSocket, RAM 캡처, 지표로 전달한다.
6. 느린 네트워크 클라이언트는 제거하며 UART 수신을 막지 않는다.

프레임 레코드는 direction, monotonic timestamp, sequence, length, raw bytes, framing reason, UART error snapshot을 가진다.

### 7.2 framing 방식

- inter-byte: 마지막 바이트 이후 설정 gap이 지나면 프레임 종료
- delimiter: 설정한 1-byte delimiter를 포함해 종료
- length-field: 검증된 offset·base·endianness에 따라 종료
- raw-chunk: 진단 전용이며 UART read chunk를 프레임 의미로 주장하지 않는다

overflow가 발생하면 현재 프레임을 폐기하고 FRAME_TOO_LONG 이벤트를 낸 뒤 다음 명확한 경계에서 재동기화한다.

### 7.3 UART 설정 범위

- baud: 300–921600, 실사용 UI에는 승인된 후보 목록과 custom 값만 노출
- data bits: 5–8
- parity: none, even, odd
- stop bits: 1 또는 2
- gap: 1–1000 ms
- 설정 변경 시 RX 큐를 비우고 CONFIG_BOUNDARY 이벤트를 남긴다.

### 7.4 송신 중재

송신은 다음 조건을 모두 만족해야 한다.

- 관리자가 인증됨
- CSRF·nonce 검증 통과
- 물리 버튼 확인으로 arm됨
- arm 유효 시간이 남음
- payload가 1–256 bytes이고 올바른 hex
- 초당 frame·byte 제한 이내
- OTA·scanner·reset·재설정 중이 아님
- 최근 수신 활동에 기반한 bus-idle guard를 만족
- TX 큐에 공간이 있음

기본값은 60초 arm, 최대 10 frames, frame 간 최소 100 ms다. 실제 충돌 규칙이 확인되기 전에는 운영 버스 송신을 승인하지 않는다.

### 7.5 실제 baud 스캐너

스캐너는 송신하지 않는다. 기본 후보는 1200, 2400, 3840, 4800, 9600, 19200, 38400, 57600, 115200이며 사용자가 후보를 명시할 수 있다.

각 후보마다 다음을 수행한다.

1. 현재 UART 설정과 큐 상태를 보존한다.
2. TX를 차단한다.
3. UART를 후보 설정으로 바꾸고 50 ms 안정화한다.
4. 최소 3초 또는 설정된 표본 시간 동안 raw bytes, UART error, gap 분포, 반복 frame length를 수집한다.
5. byte count, 오류율, 경계 일관성, 반복성을 별도 열로 기록한다.
6. confidence는 근거 지표에서 계산하며 0 bytes 후보는 0점이다.
7. 완료·취소·오류 모두 원래 설정을 복구한다.
8. 최고 점수를 자동 저장하지 않는다.

결과는 후보, bytes, frames, framing errors, parity errors, overflow, median gap, repeated-length ratio, confidence, 관측 시간을 포함한다. UI는 confidence를 “정답”이 아니라 비교 근거로 표시한다.

## 8. 네트워크 설계

### 8.1 Wi‑Fi

- 2.4 GHz STA가 정상 운전 모드다.
- 최초 등록 또는 물리 버튼 3초 hold 때만 AP를 최대 10분 연다.
- AP SSID는 DM-BRIDGE-XXXXXX 형식이다.
- AP 비밀번호는 최초 부팅에서 생성한 12자 이상 무작위 enrollment key다.
- enrollment key는 USB serial에 한 번 출력하고, 제조·설치 프로세스가 라벨 또는 비밀 저장소에 보관한다.
- STA 저장 후 AP는 자동 종료한다.
- 재연결은 5초에서 시작해 최대 60초의 지수 backoff와 jitter를 쓴다.
- hostname은 dm-bridge-XXXXXX, mDNS 이름은 동일하게 사용한다.
- Wi‑Fi 비밀번호는 어떤 API와 로그에도 반환하지 않는다.

### 8.2 raw TCP

- 기본 비활성화
- 활성화 시 기본 port 8899, 최대 2 clients
- 수신 데이터를 그대로 broadcast하는 read-only가 기본
- TCP→RS485는 별도 설정과 물리 arm이 모두 있어야 동작
- raw TCP 자체에는 사용자 인증이 없으므로 신뢰 LAN 밖에 노출하지 않는다.
- listen address, client 수, idle timeout, keepalive, 선택적 IPv4 CIDR allowlist를 설정한다.
- 이 포트는 Modbus TCP가 아니며 MBAP 변환을 하지 않는다.
- 느린 client의 send buffer가 2초 이상 막히면 연결을 끊고 SLOW_CLIENT_DROP을 계수한다.

## 9. 설정 저장과 적용

### 9.1 NVS 형식

Arduino Preferences가 사용하는 NVS에는 작은 설정만 저장한다. namespace와 key는 15자 이내로 제한한다.

권장 namespace: dmbridge  
권장 keys: cfgA, cfgB, active, schema, bootfail

각 설정 slot은 magic, schema version, generation, payload length, CRC32, canonical JSON payload로 구성한다.

저장 순서:

1. 입력 전체를 RAM에서 검증한다.
2. inactive slot에 새 generation을 쓴다.
3. 다시 읽어 length와 CRC32를 검증한다.
4. active key를 새 slot으로 전환한다.
5. 실패하면 기존 active slot을 유지한다.

부팅 때 두 slot을 모두 검증하고 가장 높은 정상 generation을 선택한다. 둘 다 실패하면 안전 기본값으로 부팅하고 CONFIG_CORRUPT 상태를 표시한다.

### 9.2 설정 영역

- device: name, timezone display option
- wifi: SSID, secret, timeout, backoff
- uart: baud, data bits, parity, stop bits, framing, gap
- tcp: enabled, mode, host, port, max clients, TX permission
- web: hostname, session timeout, console line limit
- security: username, verifier, failed-login counters
- log: serial level, remote log enabled, port
- safety: TX disabled, arm duration, rate limit, idle guard

### 9.3 적용 의미

설정 저장 응답은 다음을 반드시 구분한다.

- appliedNow: 즉시 적용된 필드
- restartRequired: 재부팅이 필요한 필드
- reconnectExpected: 현재 브라우저 연결이 끊길 필드
- configRevision: 저장된 generation
- effectiveConfig: secret이 제거된 실제 적용값

UART·TCP·로그는 검증 후 live 적용할 수 있다. Wi‑Fi SSID, hostname, 인증 방식, 파티션 관련 값은 재부팅 적용을 기본으로 한다. UI는 “저장”, “저장 후 재연결”, “저장 후 재부팅”을 서로 다른 동작으로 표시한다.

## 10. 웹 API 계약

모든 API는 /api/v1 아래에 둔다. 응답 envelope는 다음 형태다.

    {
      "ok": true,
      "data": {},
      "error": null,
      "requestId": "8hex"
    }

오류는 HTTP status와 안정된 code를 함께 반환한다. mutation은 JSON Content-Type, body 상한, 인증, Origin, CSRF nonce, configRevision을 검사한다.

| Method | Path | 기능 |
|---|---|---|
| GET | /api/v1/status | 상태·지표·effective 설정 요약 |
| GET | /api/v1/config | secret을 제거한 전체 설정 |
| PUT | /api/v1/config | 검증·저장·적용 결과 반환 |
| POST | /api/v1/wifi-scan | 주변 SSID 비동기 스캔 |
| GET | /api/v1/wifi-scan | 스캔 상태와 결과 |
| POST | /api/v1/scanner | 수동형 baud scan 시작 |
| GET | /api/v1/scanner | 진행률과 근거 지표 |
| DELETE | /api/v1/scanner | 취소 후 설정 복구 |
| POST | /api/v1/tx-arm/request | 물리 확인 대기 시작 |
| GET | /api/v1/tx-arm | arm 상태·남은 시간 |
| POST | /api/v1/tx | 제한된 raw frame 송신 |
| POST | /api/v1/reboot | 응답 flush 후 재부팅 |
| POST | /api/v1/reset-settings | 네트워크 자격 증명 유지 초기화 |
| POST | /api/v1/reset-all | 물리 확인 포함 전체 초기화 |
| POST | /api/v1/firmware/prepare | 서명 manifest 검증과 upload token |
| PUT | /api/v1/firmware/image | inactive slot으로 stream |
| GET | /api/v1/firmware | OTA 상태·rollback 정보 |
| GET | /api/v1/capture | RAM capture를 NDJSON로 export |
| GET | /ws/v1/stream | packet·status·system event |

### 10.1 status 필수 필드

- firmwareVersion, buildId, schemaVersion
- uptimeMs, bootReason, state
- wifi mode, IP, RSSI, reconnect count
- UART effective format, rawBytes, rxFrames, txFrames
- framingError, uartOverflow, frameOverflow, queueOverflow
- RX/TX/capture queue depth와 실제 capacity
- TCP client count, dropped clients, bytes in/out
- WebSocket client count, dropped events
- authFailures, lastErrorCode
- configRevision, restartRequired
- txArmed, txArmExpiresMs, txBudgetRemaining
- scanner state, OTA state, freeHeap, largestFreeBlock

## 11. 웹 대시보드

UI는 외부 CDN·font·analytics 없이 firmware와 원자적으로 배포되는 gzip asset으로 제공한다.

### 11.1 화면

- Overview: 연결, UART, 트래픽, 큐, 메모리, 오류, 버전
- Live Console: RX/TX/system 색상 구분, pause, filter, clear, NDJSON export
- Bridge: TCP mode, clients, read-only/TX 정책
- Serial: UART와 framing 설정, 검증 결과
- Wi‑Fi: STA 설정, scan, AP 남은 시간, 재연결 경고
- Baud Scan: 후보, 진행률, 실측 열, 복구 상태
- Security: 계정 변경, 실패 횟수, 세션 종료, 네트워크 노출 경고
- Maintenance: signed OTA, reboot, 설정 reset, 전체 reset
- About: build ID, source provenance, 라이선스, 공식 문서 링크

### 11.2 상태 표현

- WebSocket 단절 시 실제 숫자를 0으로 바꾸지 않고 마지막 갱신 시각과 STALE을 표시한다.
- simulation·demo는 production build에 넣지 않는다.
- 카운터는 서버가 보낸 단조 증가값만 사용한다.
- TX 입력은 arm 전에는 DOM 수준에서 disabled이고 서버도 독립적으로 거부한다.
- scanner 결과가 없으면 “관측 데이터 없음”으로 표시한다.
- 저장 뒤 실제 적용 필드와 재부팅 필요 필드를 결과 panel에 분리한다.
- 파괴적 동작은 결과와 복구 방법을 설명한 뒤 재인증·확인 문구를 요구한다.

### 11.3 접근성·반응형

- 키보드만으로 모든 기능 사용 가능
- visible focus, semantic label, ARIA live region
- 최소 44×44 px touch target
- 텍스트 대비 WCAG AA
- 색상만으로 RX/TX/오류를 구분하지 않음
- 320 px부터 desktop까지 수평 overflow 없음
- prefers-reduced-motion 지원
- console 자동 스크롤을 사용자가 중지할 수 있음

## 12. 보안 설계

### 12.1 위협 모델

보호 대상은 RS485 송신 권한, Wi‑Fi secret, 관리자 자격 증명, firmware 무결성, 설정, 운영 가용성이다. 기본 배포는 신뢰 LAN을 가정하지만 같은 LAN의 비신뢰 client와 웹 기반 CSRF는 공격자로 본다. 물리 flash 추출과 고급 RF 공격은 기본 profile 밖이며 별도 hardening으로 다룬다.

### 12.2 필수 통제

- admin/admin 같은 공통 기본 계정 금지
- 최초 자격 증명은 장치별 무작위 생성
- 계정 변경 UI와 모든 세션 무효화 제공
- 로그인 실패 rate limit과 지수 지연
- API, WebSocket handshake, OTA 모두 동일한 인증 정책
- request body와 upload callback은 첫 chunk를 저장하거나 부작용을 내기 전에 명시적으로 인증
- mutation은 Origin allowlist와 boot-scoped CSRF nonce 검사
- GET endpoint는 상태를 바꾸지 않음
- body·header·upload size 상한
- secret, Authorization, cookie, Wi‑Fi key 로그 금지
- raw TCP read-only 기본, WAN port-forward 금지
- TX arm은 물리 버튼 확인과 짧은 만료
- reboot·reset·OTA 중 TX 강제 차단
- security event는 counter와 reason code로 남김

### 12.3 인증 profile

장치 자체는 HTTP Digest를 우선 사용하고 장치별 secret의 verifier만 저장한다. Chrome·Safari·WebSocket 호환성은 M5에서 실장 검증한다. WebSocket 인증 재사용이 브라우저에서 불안정하면 인증된 API가 발급한 30초·1회용 subprotocol token을 사용한다.

평문 HTTP가 안전한 채널을 제공하지는 않으므로 장치를 인터넷에 직접 노출하지 않는다. 비신뢰 네트워크나 원격 접속은 VPN 또는 TLS reverse proxy 뒤에 둔다. 제품 보안 profile에서는 ESP32 secure boot와 flash encryption을 제조 공정에서 함께 평가한다.

### 12.4 signed OTA

1. manifest는 version, boardId, imageSize, sha256, minSchema, buildId를 포함한다.
2. manifest의 Ed25519 서명을 firmware 내 public key로 검증한다.
3. 성공 시 짧은 upload token을 발급한다.
4. image는 inactive OTA slot에 stream하며 size와 SHA-256을 검사한다.
5. 검증 전 boot partition을 변경하지 않는다.
6. 재부팅 후 30초 안에 UART, 설정, Wi‑Fi task가 정상임을 표시해야 valid로 확정한다.
7. watchdog reset 또는 health 미확정이면 이전 slot으로 rollback한다.
8. downgrade는 명시적 recovery mode에서만 허용한다.

## 13. LED와 버튼 UX

LED 우선순위는 reset > OTA > TX arm > fault > Wi‑Fi > traffic > idle 순이다.

| 상태 | LED |
|---|---|
| boot | dim white |
| Wi‑Fi connecting | amber blink |
| provisioning AP | blue slow blink |
| RX activity | green pulse |
| TX activity | cyan pulse |
| TX arm 대기 | amber fast blink |
| TX armed | amber solid |
| degraded | red blink |
| OTA | purple pulse |
| reset 확정 | red solid |
| idle 정상 | 매우 약한 white 또는 off |

버튼 동작:

- runtime short press: pending TX arm 확인. 대기 요청이 없으면 아무 동작 없음.
- runtime 3초 hold: TX를 해제하고 10분 provisioning AP 진입.
- power-on 8초 hold: 전체 공장 초기화. 5초부터 red fast blink, 8초에 red solid.
- OTA 중 버튼의 reset·provisioning 동작은 무시하고 전원 차단 금지를 LED로 알린다.

## 14. 복구와 운영

### 14.1 reset

- settings reset: UART, TCP, UI, 로그, 안전 설정을 기본값으로 복구하되 STA와 관리자 자격 증명은 유지
- full reset: 모든 NVS slot, Wi‑Fi, 관리자 자격 증명, enrollment key를 지우고 새 key를 생성
- full reset은 power-on 물리 hold 또는 웹 재인증 + 확인 문구 + 30초 안의 물리 버튼 확인이 필요
- reset 응답을 client에 보낸 뒤 500 ms 이상 flush 시간을 두고 실행

### 14.2 watchdog과 부팅 실패

- 정상 event loop heartbeat로 task watchdog을 feed
- 연속 3회 boot health 실패 시 recovery profile로 부팅
- recovery profile은 TX, TCP, scanner를 끄고 provisioning과 signed OTA만 제공
- bootReason, failing buildId, rollback count를 NVS의 작은 counter로 보존
- counter는 부팅·OTA 사건에만 써서 flash wear를 제한

### 14.3 로그

- USB serial은 개발·현장 진단의 기준 로그
- 선택적 TCP log는 기본 비활성화하고 최대 client 1
- log entry는 monotonic time, level, subsystem, code, 정수 context만 포함
- raw payload는 사용자가 console capture를 켰을 때만 RAM에서 보유
- 영구 packet replay는 제공하지 않는다.

## 15. 파티션과 빌드

### 15.1 4 MB flash 기준

- NVS: 20 KiB 이상
- OTA metadata: 8 KiB
- app0: 1.5 MiB
- app1: 1.5 MiB
- 나머지: partition table 정렬, bootloader, 최소 진단 영역, 예비 공간
- 정상 동작이 filesystem에 의존하지 않도록 web asset은 app image에 포함
- build가 1.25 MiB를 넘으면 기능을 추가하기 전에 asset과 dependency를 줄인다.

### 15.2 빌드 재현성

- PlatformIO와 Arduino-ESP32의 정확한 version을 lock한다.
- library는 exact version 또는 commit SHA와 SHA-256을 기록한다.
- floating branch, latest tag, 원격 asset을 build input으로 쓰지 않는다.
- release artifact에는 firmware.bin, signed manifest, SBOM, build log, compiler version, source commit, test report를 포함한다.
- release build는 warnings as errors, LTO, production log level을 사용한다.
- target 환경은 하나만 유지한다.

## 16. 시험 전략

### 16.1 host 시험

- framing: gap 경계, delimiter 포함, length offset, wraparound time, 0 byte, 최대 길이, 초과 길이
- queue: full/empty, 순서, overflow counter, producer/consumer interleave
- config: 범위 검증, schema migration, CRC 불일치, 한 slot 손상, 저장 중 전원 차단
- hex input: 공백, 홀수 nibble, invalid char, 0 length, 256/257 bytes
- TX policy: arm 없음, 만료, budget 초과, scanner/OTA 중, idle guard
- scanner score: 0 bytes=0, 오류 패널티, 동일 입력의 결정성, 원래 설정 복구
- API: secret redaction, revision conflict, body limit, error envelope
- auth: body 첫 chunk 이전 거부, upload 첫 chunk 이전 거부, CSRF, Origin, rate limit

### 16.2 하드웨어 통합 시험

- GPIO26/32 UART loopback
- base를 통한 A/B 송수신과 방향 release
- RX-only 실버스 캡처와 로직 분석기 byte-for-byte 비교
- 1200, 3840, 9600, 115200에서 1시간 무손실
- Wi‑Fi disconnect, DHCP 변경, AP timeout
- TCP 2 clients와 WebSocket 2 clients 동시 부하
- slow client, reconnect storm, malformed JSON, oversized body
- button timing 경계와 bounce
- USB/버스 전원 시나리오
- OTA 정상, 서명 오류, hash 오류, truncate, 전원 차단, rollback
- NVS 쓰기 중 전원 차단
- factory reset 뒤 secret 재생성

### 16.3 현장 시험

- 먼저 30분 완전 수신 전용 관측
- 기준 장비와 동시에 capture하고 bytes·timestamp를 대조
- scanner가 관측 가능한 실제 baud를 상위 후보로 내는지 확인
- 승인된 시험 frame 1개만 저부하 시간에 송신
- 상대 장치 응답과 기존 시스템 상태를 별도 관측
- 72시간 soak 동안 reboot, heap 감소, queue overflow, packet loss가 없어야 함

### 16.4 정량 합격 기준

- 선택된 현장 baud에서 72시간 예상치 못한 reboot 0
- 기준 캡처 대비 RX byte loss 0
- 정상 부하 queue overflow 0
- steady free heap 감소 추세 0, 최저 50 KiB 이상
- LAN 수신→WebSocket 표시 latency p95 100 ms 이하
- Wi‑Fi 복구 60초 이내
- 인증 없는 mutation 성공 0
- 잘못된 OTA image boot 0
- full reset 후 이전 secret 재사용 성공 0
- 접근성 keyboard blocker 0
- P0/P1 미해결 defect 0

## 17. 단계별 마일스톤

### M0 — 클린룸 기준선과 증거 봉인

목표: 구현자가 기존 코드를 보지 않고도 작업할 수 있는 증거 세트를 만든다.

작업:

- 역할과 접근 권한 분리
- 공식 문서 PDF·URL·수집일·해시 기록
- 대상 보드와 베이스 SKU·리비전 사진 기록
- DM-D5102Q 버스 배선도와 위험 승인자 지정
- 수신 전용 capture 절차와 파일 형식 정의
- 요구사항 ID와 시험 ID 등록
- toolchain·dependency version 조사 후 lock 후보 결정

산출물: evidence register, threat model, wiring checklist, initial test vectors, requirement baseline.

종료 조건:

- 구현 담당 입력에 레거시 소스가 없음
- 모든 확정 하드웨어 사실에 PUB 근거가 있음
- 모든 미확정 현장 값에 UNV와 검증 방법이 있음
- TX 금지와 stop condition을 관계자가 승인함

중단 조건: SKU·회로 리비전·버스 소유자·안전 승인 중 하나라도 불명확하면 M1로 가지 않는다.

### M1 — Atom Lite 하드웨어 bring-up과 수신 전용 UART

목표: 실제 배선에서 단 한 바이트도 송신하지 않고 전원·핀·수신을 확인한다.

작업:

- 최소 boot, serial log, LED, 버튼 구현
- GPIO26/32 loopback과 base 방향 회로 실측
- UART RX ring과 error counter 구현
- 로직 분석기와 MCU raw byte 비교
- A/B 극성, ground, 종단, 전원 시나리오 확인
- flash partition과 메모리 기준선 측정

산출물: RX-only firmware, pin proof, electrical report, raw capture.

종료 조건:

- 공식 핀 계약과 로직 분석기 결과 일치
- 1시간 115200 시험 byte loss 0
- 현장 passive capture 30분에서 버스 장애 0
- app/heap budget 충족

중단 조건: 역급전, 과열, 공통 모드 초과, 불명확한 방향 전환, 기존 시스템 오류가 하나라도 발생하면 배선을 분리한다.

### M2 — 프레이밍, 고정 큐, 상태 지표

목표: raw bytes를 근거가 있는 프레임으로 조립하고 손실을 계측한다.

작업:

- inter-byte framing 우선 구현
- delimiter와 length mode는 승인된 vector가 있을 때만 구현
- fixed RX/TX/capture queue
- sequence, timestamp, overflow reason
- config boundary와 재동기화
- host test와 fuzz input

산출물: frame engine, queue, metrics schema, vector suite.

종료 조건:

- 경계 시험과 wraparound 시험 통과
- 256-byte 상한에서 memory allocation 0
- overflow가 원인별 counter로 노출
- reference capture byte-for-byte 재현

중단 조건: 실제 최대 frame이 256 bytes를 넘으면 상한을 임의 변경하지 말고 RAM 예산을 다시 승인한다.

### M3 — read-only 네트워크 브리지

목표: RS485 수신을 로컬 client에 전달하되 네트워크에서 송신은 못 하게 한다.

작업:

- STA 연결과 backoff
- 시간 제한 provisioning AP
- raw TCP read-only server
- WebSocket packet stream
- slow client 제거와 client 상한
- Wi‑Fi loss/recovery 시험

산출물: headless read-only bridge, network test report.

종료 조건:

- TCP·WebSocket 동시 부하에서 RX loss 0
- slow client가 UART를 block하지 않음
- AP가 제한 시간 뒤 종료
- raw TCP inbound bytes가 UART에 도달하지 않음

중단 조건: 네트워크 혼잡으로 RX queue overflow가 발생하면 UI 기능을 추가하기 전에 fan-out과 backpressure를 고친다.

### M4 — 원자적 설정과 프로비저닝

목표: 전원 장애에도 복구되는 설정과 명확한 적용 의미를 제공한다.

작업:

- two-slot NVS와 CRC32
- schema validation·migration
- 장치별 enrollment secret
- settings/live/reboot 적용 분류
- serial 기반 최초 설치 절차
- power-cut fault injection

산출물: config schema, provisioning flow, recovery report.

종료 조건:

- 저장 단계별 전원 차단에서 이전 또는 새 설정으로 정상 부팅
- secret이 API·로그에 노출되지 않음
- Wi‑Fi 변경 응답이 reconnect/reboot를 정확히 알림
- corrupt 양 slot에서 안전 기본 상태로 부팅

중단 조건: 설정 손상으로 TX가 활성화되거나 AP가 무기한 열리면 다음 단계 금지.

### M5 — 웹 대시보드와 진단

목표: 실제 상태만 표시하는 접근 가능한 관리 UI를 제공한다.

작업:

- versioned REST API와 error envelope
- Overview, Console, Serial, Wi‑Fi, Bridge 화면
- stale/offline 상태
- NDJSON RAM capture export
- accessibility와 mobile layout
- body size·JSON validation

산출물: gzip UI asset, API contract test, accessibility checklist.

종료 조건:

- 모든 숫자가 device status에서 유래
- WebSocket 단절 때 fake 0 또는 fake traffic 없음
- keyboard-only 핵심 작업 가능
- 320 px 화면 overflow 없음
- body 4097 bytes가 부작용 없이 거부됨

중단 조건: UI label과 실제 적용 동작이 다르면 송신·OTA 기능을 올리지 않는다.

### M6 — 물리 확인형 제한 송신

목표: 승인된 운영자가 짧은 창 안에서만 검증된 frame을 보낸다.

작업:

- TX policy와 arm state machine
- physical button confirmation
- idle guard, frame·byte rate limit
- TCP inbound TX 별도 gate
- collision·timeout·queue full 처리
- 감사 이벤트와 LED

산출물: controlled TX, safety test report, 현장 승인 기록.

종료 조건:

- reboot·disconnect·timeout에서 arm 즉시 해제
- 인증·CSRF·물리 확인 중 하나라도 없으면 UART write 0
- 승인된 단일 frame 현장 시험 성공
- 송신 후 기존 시스템 상태 이상 없음

중단 조건: 버스 충돌 규칙이나 상대 장치 영향이 불명확하면 read-only release로 유지한다.

### M7 — 실측형 baud 스캐너

목표: 합성값 없이 실제 수신 통계로 UART 후보를 비교한다.

작업:

- 후보 목록과 표본 시간
- UART error·gap·frame 반복성 수집
- 결정적 confidence 계산
- cancel/error/power event 복구
- UI 근거 열과 “추정” 표시

산출물: passive scanner, captured evidence set, scoring tests.

종료 조건:

- 0-byte 후보 score 0
- 동일 capture에서 동일 결과
- scan 중 TX 0
- 모든 종료 경로에서 원래 UART 설정 복구
- 알려진 signal의 실제 baud가 상위 후보에 포함

중단 조건: UART driver가 framing/parity error를 노출하지 못하면 해당 열을 0으로 위조하지 말고 unavailable로 표시한다.

### M8 — 인증, signed OTA, reset, rollback

목표: 관리 경계와 firmware 복구를 출시 수준으로 만든다.

작업:

- Digest 또는 승인된 대체 인증의 browser 검증
- body/upload 첫 chunk 전 auth guard
- CSRF·Origin·rate limit
- signed manifest와 inactive-slot OTA
- health confirmation·rollback
- reset 재인증과 물리 확인
- dependency advisory 점검

산출물: security test report, signed release candidate, recovery matrix.

종료 조건:

- 인증 없는 body·upload 부작용 0
- wrong signature/hash/size image가 boot되지 않음
- OTA 전원 차단 뒤 이전 image 부팅
- full reset이 이전 credential을 제거
- Chrome·Safari에서 API와 WebSocket 인증 통과

중단 조건: upload callback이 인증 전에 flash를 쓰거나 boot partition을 바꾸면 즉시 release 금지.

### M9 — 현장 내구, 문서, 출시

목표: 실제 설치 조건에서 검증된 재현 가능한 release를 만든다.

작업:

- 72시간 soak
- Wi‑Fi·전원·client fault injection
- resource trend 분석
- 설치·복구·rollback runbook
- SBOM, license, source·artifact provenance
- release checklist와 운영 인계

산출물: v1.0 artifact bundle, test evidence, installer runbook, rollback image.

종료 조건:

- 정량 합격 기준 전부 통과
- P0/P1 defect 0
- firmware와 manifest signature 검증 가능
- 현장 설치자가 문서만으로 RX-only 설치·복구 수행
- 승인자가 TX profile 또는 RX-only profile을 명시적으로 선택

중단 조건: live device, 실제 배선, DM-D5102Q 응답 검증이 없으면 “현장 검증 완료”로 표기하지 않는다.

## 18. 요구사항 추적표

| 요구사항 | 마일스톤 | 핵심 검증 |
|---|---|---|
| HW-01 전원·핀·종단 | M0–M1 | 회로·파형·온도 |
| RX-01 무손실 raw 수신 | M1–M3 | 로직 분석기 대조 |
| FR-01 근거 기반 framing | M2 | vector·fuzz |
| NET-01 read-only fan-out | M3 | slow client·동시 부하 |
| CFG-01 원자적 설정 | M4 | power-cut |
| WEB-01 실제 상태 UI | M5 | API provenance·offline |
| TX-01 물리 확인 송신 | M6 | negative policy tests |
| SCN-01 실측 scanner | M7 | known-signal capture |
| SEC-01 모든 mutation 보호 | M8 | first-chunk 공격 |
| OTA-01 서명·rollback | M8 | corrupt·power-cut |
| OPS-01 72시간 안정성 | M9 | soak·resource trend |

## 19. 주요 위험과 대응

| 위험 | 영향 | 대응 |
|---|---|---|
| UART 기본값이 실제 장치와 다름 | 무수신·오판 | passive capture와 scanner, 자동 저장 금지 |
| A/B 라벨 차이 | 무수신 | 전원 off continuity·파형 검사 |
| 잘못된 종단 | 반사·버스 장애 | topology 문서화, endpoint에만 120 Ω |
| USB·버스 전원 역급전 | 손상 | 단일 전원 시운전, 동시 연결 실측 gate |
| 자동 방향 전환 불확실 | 충돌 | scope 측정 전 TX 금지 |
| Wi‑Fi client가 UART를 block | packet loss | 고정 큐, non-blocking fan-out, slow client drop |
| raw TCP 무인증 | 임의 송신 | read-only 기본, TX arm, 신뢰 LAN |
| 평문 HTTP | credential 노출 | 직접 인터넷 노출 금지, VPN/TLS proxy |
| 설정 저장 중 전원 차단 | 부팅 실패 | A/B slot, CRC, generation |
| OTA 위조·중단 | takeover·brick | Ed25519, inactive slot, rollback |
| scanner false positive | 잘못된 설정 | 근거 열, confidence 표기, 사용자 선택 |
| flash/RAM 부족 | 불안정 | build·heap gate, dependency 최소화 |
| capture의 개인정보·운영정보 | 정보 노출 | RAM only, 명시적 export, secret masking |

## 20. 출시 정의

v1.0은 다음 조건을 모두 만족할 때만 완료다.

- 대상 보드 하나의 production build만 존재한다.
- 공식 핀·전원·종단 근거와 현장 측정 기록이 있다.
- 실제 DM-D5102Q 버스에서 RX byte loss 0이 증명됐다.
- TX profile은 별도 승인됐거나 명시적으로 비활성 release다.
- synthetic data와 automatic replay가 없다.
- 설정 저장, 인증, OTA, reset의 실패 경로가 시험됐다.
- body/upload first-chunk 인증 공격이 차단됐다.
- scanner는 실제 관측값만 반환한다.
- 72시간 soak와 resource gate를 통과했다.
- artifact, manifest, SBOM, test report, 설치·복구 문서가 한 release ID로 연결된다.
- 미검증 사항은 release note에 UNV로 남아 있고 검증 완료로 과장되지 않는다.

## 21. 구현 착수 체크리스트

- [ ] M0 역할 분리와 증거 register 승인
- [ ] 정확한 Atom Lite와 Atomic RS485 Base SKU·revision 사진
- [ ] 버스 소유자와 수신 전용 작업 승인
- [ ] 전원·A/B·GND·종단 topology 측정
- [ ] GPIO26 TX / GPIO32 RX / 자동 방향 실측
- [ ] passive raw capture와 SHA-256
- [ ] DM-D5102Q UART 후보와 미검증 표시
- [ ] toolchain·library exact version lock
- [ ] TX compile-time off bring-up build
- [ ] stop condition과 복구 담당자 확인

## 22. 공식 근거

- [M5Stack Atom Lite 공식 문서](https://docs.m5stack.com/en/core/ATOM%20Lite)
- [M5Stack Atomic RS485 Base 공식 문서](https://docs.m5stack.com/en/atom/Atomic%20RS485%20Base)
- [M5Stack Atomic RS485/232 Base Arduino 튜토리얼](https://docs.m5stack.com/en/arduino/projects/atomic/atomic_rs485_232_base)
- [Espressif Arduino Preferences 공식 문서](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html)
- [ESPAsyncWebServer 공식 저장소](https://github.com/ESP32Async/ESPAsyncWebServer)

## 23. 현재 검증 상태

문서 작성 시 검증됨:

- 공식 Atom Lite 자원·핀·Wi‑Fi 사양
- 공식 Atomic RS485 Base 변환기·전원·종단저항 부재
- 프로젝트의 현재 Atom Lite 기능 표면과 피해야 할 인증·설정·scanner 결함
- 4 MB flash에서 이중 OTA slot을 둘 수 있는 현재 파티션 기준선
- 웹 body/upload callback이 전역 middleware만으로 보호되지 않을 수 있다는 현행 라이브러리 경계

아직 미검증:

- 실제 Atom Lite와 선택한 base revision의 전기·방향 동작
- DM-D5102Q 현장 baud·frame·A/B·응답
- 실제 장치의 송신 안전성
- browser별 Digest/WebSocket 조합
- signed OTA, rollback, power-cut recovery의 실장 결과
- 72시간 soak와 현장 packet-loss 수치

이 미검증 항목은 M1, M6, M8, M9의 종료 gate를 통과하기 전까지 완료로 보고하지 않는다.
