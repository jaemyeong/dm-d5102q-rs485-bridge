# Wi-Fi Provisioning
> 🌐 Read this in: **한국어** · [English](./wifi-provisioning.en.md)

## PRD 매핑

- §5.1 Wi-Fi Provisioning
- §5.2 Wi-Fi 연결 관리
- §6 NVS 저장

## AP 진입 조건

| 조건 | 동작 |
|---|---|
| 저장된 SSID 없음 | AP 모드 시작 |
| Wi-Fi 연결 재시도 초과 | AP fallback |
| Factory Reset 직후 | NVS clear 후 AP 모드 |
| 사용자 설정 진입 | TODO |

## 웹 폼 필드

| 필드 | 설명 | 검증 |
|---|---|---|
| SSID | 연결할 Wi-Fi 이름 | 1-32 bytes TODO |
| PSK | Wi-Fi 비밀번호 | 8-63 bytes 또는 open TODO |
| Hostname | 장치 이름 | mDNS 도입 시 확장 |
| Basic Auth 사용자 | Web UI 인증 | 적용 범위 TODO |
| Basic Auth 비밀번호 | Web UI 인증 | 최소 길이 TODO |

## Fallback 정책

- 재시도 횟수와 backoff는 후속 구현에서 확정합니다.
- AP 모드에서도 Dashboard에 연결 실패 원인을 표시합니다.
- 성공적으로 STA에 연결되면 AP를 끌지 동시 운영할지 결정이 필요합니다.

## TODO

- AP SSID/비밀번호 기본값 확정
- STA+AP 동시 운영 여부 결정
- 저장 전 SSID scan 결과 표시 여부 결정
