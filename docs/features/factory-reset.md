# Factory Reset
> 🌐 Read this in: **한국어** · [English](./factory-reset.en.md)

## PRD 매핑

- §5.19 Factory Reset
- §6 NVS 저장
- §5.1 Wi-Fi Provisioning

## 조건

| 입력 | 동작 |
|---|---|
| BtnA 5초 hold | NVS clear 후 재부팅 |
| HTTP API 요청 | 인증 후 NVS clear TODO |
| 부팅 중 특정 조건 | 필요 여부 TODO |

## 흐름

1. BtnA hold 시간을 측정합니다.
2. 5초에 가까워지면 LED를 Reset 패턴으로 표시합니다.
3. 조건 충족 시 NVS namespace를 clear합니다.
4. 재부팅합니다.
5. 다음 부팅에서 AP mode로 진입합니다.

## 보존/삭제 범위

| 데이터 | 처리 |
|---|---|
| Wi-Fi credentials | 삭제 |
| TCP settings | 삭제 |
| UART settings | 기본값 복원 |
| Basic Auth | 기본값 복원 TODO |
| Packet Replay | RAM only라 재부팅으로 삭제 |

## TODO

- HTTP factory reset endpoint 제공 여부 확정
- BtnA pin 실측 후 보드별 설정 반영
- reset confirmation UX 확정
