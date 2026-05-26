# 보안 설계
> 🌐 Read this in: **한국어** · [English](./security.en.md)

## 위협 모델

이 장치는 로컬 네트워크 또는 임시 AP에서 사용되는 RS485 디버깅·자가 진단 도구입니다. 주요 위험은 인증되지 않은 사용자가 웹 UI, WebSocket, TCP Bridge, OTA, HEX TX 기능에 접근하는 상황입니다.

## 보호 대상

| 대상 | 위험 | 대응 방향 |
|---|---|---|
| Wi-Fi 자격 증명 | NVS 유출 | 로그 출력 금지, 설정 화면 masking |
| HEX TX | 의도치 않은 프레임 송신 | 인증, UI 확인, 감사 로그 TODO |
| OTA | 악성 펌웨어 업로드 | Basic Auth, 로컬망 제한 |
| WebSocket | 평문 트래픽 노출 | 신뢰 네트워크, 인터넷 노출 금지 |
| TCP Bridge | 외부 클라이언트 오남용 | 포트 제한, 접속 수 제한 |

## AP 비밀번호 정책

- AP 모드는 초기 설정과 Wi-Fi fallback에 사용합니다.
- 기본 AP 비밀번호는 후속 구현에서 확정합니다.
- 빈 비밀번호 AP는 개발 빌드에서만 허용하는 방향을 검토합니다.

## 웹 인증 정책

0.1.9부터 모든 웹 접근에 HTTP Basic Auth가 강제됩니다. 적용 범위는 다음과 같습니다.

- 정적 어드민 UI(`/`, `/dashboard`, `/console`, `/ota`, `/about`, 기타 미정의 경로)
- 모든 `/api/*` 엔드포인트(읽기/쓰기 무관)
- OTA 펌웨어 업로드(`/update`)
- WebSocket(`/ws/console`) 핸드셰이크

인증되지 않은 요청은 `401 Unauthorized`와 `WWW-Authenticate: Basic realm="DM-D5102Q Bridge"` 헤더를 받습니다.

기본 자격증명은 NVS에 저장된 값 → `secrets.h`의 `BRIDGE_DEFAULT_WEB_USER` / `BRIDGE_DEFAULT_WEB_PASSWORD` → 컴파일 기본값 `admin/admin` 순서로 적용됩니다. 첫 부팅 후 즉시 비밀번호를 변경하세요.

자격증명을 잊으면 부팅 시 버튼 5초 홀드로 설정만 초기화하거나(WiFi 정보 유지) 8초 이상 홀드로 NVS 전체 초기화로 복구합니다.

## 인터넷 노출 시 권장 사항

- 직접 포트 포워딩을 하지 않습니다.
- 원격 접근이 필요하면 VPN 또는 SSH 터널을 사용합니다.
- WebSocket과 TCP Bridge는 평문으로 간주합니다.

## TODO

- HEX TX 권한 분리 여부 결정
- AP 비밀번호 기본값과 강도 규칙 확정
