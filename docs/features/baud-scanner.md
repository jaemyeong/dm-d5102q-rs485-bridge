# Baud Scanner
> 🌐 Read this in: **한국어** · [English](./baud-scanner.en.md)

## PRD 매핑

- §5.4 Baud Rate 정책
- §5.14 Baud Scanner

## 페이지

| 경로 | 설명 |
|---|---|
| `/scanner` | baud 범위 sweep과 품질 평가 UI |
| `/api/scanner/start` | scanner 시작 |
| `/api/scanner/result` | 결과 조회 |

## 입력 필드

| 필드 | 설명 | 기본값 |
|---|---|---|
| start | 시작 baud | TODO |
| end | 종료 baud | TODO |
| step | 증가 폭 또는 preset | TODO |
| sample | baud별 수집 시간/프레임 수 | TODO |
| parity | parity 후보 | current setting |

## 동작 흐름

1. 현재 UART 설정을 저장합니다.
2. Console RX 제한 정책을 적용합니다.
3. 각 baud에서 sample window 동안 프레임 품질을 평가합니다.
4. 점수, 프레임 수, printable/HEX 안정성, overflow를 결과에 기록합니다.
5. 원래 UART 설정으로 복귀합니다.

## 결과 표

| baud | frames | bytes | score | overflow | note |
|---:|---:|---:|---:|---:|---|
| TODO | TODO | TODO | TODO | TODO | TODO |

## TODO

- Console RX를 scanner 중 강제 제한할지 사용자 옵션으로 둘지 결정
- 품질 점수 계산식 확정
- preset 목록과 custom 범위 UI 확정
