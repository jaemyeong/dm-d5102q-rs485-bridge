# 보안 정책
> 🌐 Read this in: **한국어** · [English](./SECURITY.en.md)

## 신고 경로

보안 취약점은 공개 이슈 대신 아래 경로로 신고해 주세요.

- 이메일: `jaemyeong@me.com`
- GitHub Private Vulnerability Reporting

## 응답 SLA

- 수신 확인: 72시간 이내
- 1차 평가: 7일 이내
- 패치 또는 완화 계획: 30일 이내

## 적용 범위

- Wi-Fi, TCP, WebSocket, HTTP 엔드포인트
- OTA 업로드 경로와 인증 흐름
- 웹 UI와 수동 HEX 프레임 전송 기능
- NVS 저장 설정과 민감 설정 처리
- Arduino/ESP32 빌드 체인과 배포 산출물

## 적용 범위 외

- DM-D5102Q 월패드 자체 펌웨어나 외부 서비스
- 제3자 라이브러리의 upstream 취약점 자체
- 사용자 네트워크 구성, 전원, 배선 문제

## 알려진 보안 한계

- AP 비밀번호와 Basic Auth는 로컬 진단 장비를 위한 최소 보호 계층입니다.
- 기본 웹 UI와 WebSocket은 HTTPS/TLS를 제공하지 않습니다.
- HEX TX는 실제 RS485 버스에 프레임을 송신할 수 있으므로 권한 있는 사용자만 접근해야 합니다.
- 인터넷에 직접 노출하지 말고, 필요한 경우 VPN 또는 격리된 관리망을 사용하세요.

## 지원 버전

| 버전 | 상태 |
|---|---|
| 0.1.x | 초기 개발, 보안 수정 수용 |
