# 변경 기록
> 🌐 Read this in: **한국어** · [English](./CHANGELOG.en.md)

이 프로젝트는 [Keep a Changelog](https://keepachangelog.com/ko/1.1.0/) 형식을 따르고, 버전 번호는 [Semantic Versioning](https://semver.org/lang/ko/)을 따릅니다.

## [Unreleased]

## [0.1.10] - 2026-05-29
### Added
- 웹 관리자 헤더 재설계: 46px 토픽바 + 클릭으로 펼치는 상세 drawer.
- 헬스 요약(`정상/경고/장애/오프라인`)과 활성 알림 개수를 헤더 우측에서 항상 표시.
- 헤더에서 바로 호출 가능한 **재부팅** 버튼 (확인 모달 + 자동 폴링 재접속).
- `GET /api/info` — 펌웨어 버전, 커밋, 빌드 시각, 보드, TCP 최대 클라이언트, 큐 용량.
- `POST /api/reboot` — 인증 보호된 재부팅 API (200 → 500ms 후 `ESP.restart()`, 중복 호출 시 409).

### Changed
- `GET /api/status` `metrics`에 `auth_failures`, `last_auth_fail_ago_ms` 추가.
- `data.device`에 `commit`, `built_at` 노출.
- 펌웨어 버전 0.1.10으로 상향.

### Security
- 인증 실패 횟수를 펌웨어에서 추적하고 헤더 알림으로 노출.

## [0.1.9] - 2026-05-27

### Changed
- 웹 어드민과 프로비저닝 화면이 모든 경로(정적 HTML, `/api/*`, `/update` OTA, `/ws/console` WebSocket)에서 HTTP Basic Auth를 강제합니다.
- 설정 스키마와 어드민 UI에서 `basic_auth` 토글 필드를 제거했습니다. 인증은 항상 적용됩니다.

### Security
- `/update` OTA 업로드 엔드포인트가 이제 인증을 요구합니다(이전에는 자격증명 없이 접근 가능).
- `/ws/console` WebSocket 핸드셰이크가 이제 인증을 요구합니다.
- 모든 GET 엔드포인트(`/api/status`, `/api/config`, `/api/wifi/scan`, `/api/scanner/result`)가 이제 인증을 요구합니다.

## [0.1.2] - 2026-05-26

### Added

- 관리자 페이지를 펌웨어에 gzip 내장하여 단일 OTA 펌웨어 bin으로 배포
- 설정 화면의 Wi-Fi 감지 네트워크 목록과 SSID 선택 기능
- OTA 성공 후 자동 새로고침 예약

### Changed

- OTA 업로드는 펌웨어 이미지 하나만 플래시하도록 단순화

## [0.1.1] - 2026-05-26

### Added

- 경로 기반 관리자 페이지 라우팅과 한국어 관리자 UI
- 펌웨어 OTA와 LittleFS 웹 UI OTA 업로드 구분 처리
- 프로비저닝 저장 후 재부팅 안내 화면

- 저장소 설정, 라이센스, 커뮤니티 문서 골격
- Arduino IDE 2.x 기반 AtomS3-Lite / Atom-Lite 펌웨어 디렉터리 구조
- `shared/` 마스터 트리와 보드별 단방향 동기화 정책
- RS485, TCP, WebSocket, HTTP API 문서 골격
- Wi-Fi Provisioning, Baud Scanner, Packet Replay, Web Console, OTA, Status LED, Dashboard, Factory Reset 기능 문서 골격
- PC 도구, 테스트, 캡처, 하드웨어 문서 디렉터리 골격

### Changed

- 프로비저닝 화면은 AP/공장초기화 상태에서만 표시하고 일반 관리자 메뉴에서는 제거
- Wi-Fi 설정과 공장 초기화 버튼을 관리자 설정 화면에 배치

## [0.1.0] - TBD

초기 공개 버전 예정.
