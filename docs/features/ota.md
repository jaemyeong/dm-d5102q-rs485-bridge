# OTA Update
> 🌐 Read this in: **한국어** · [English](./ota.en.md)

## PRD 매핑

- §5.16 OTA Update
- §9 AP 비밀번호 + Basic Auth

## 통합 방향

ElegantOTA를 사용해 `.bin` 펌웨어 업로드를 지원합니다. OTA는 로컬 진단 장비 유지보수를 위한 기능이며, 인터넷 직접 노출을 전제로 하지 않습니다.

## 업로드 흐름

1. 사용자가 인증된 OTA 페이지에 접속합니다.
2. 보드에 맞는 `.bin` 파일을 선택합니다.
3. 업로드 중 Status LED를 OTA 패턴으로 전환합니다.
4. 업로드 완료 후 재부팅합니다.
5. Dashboard에서 새 버전을 확인합니다.

## 검증 항목

| 항목 | 설명 |
|---|---|
| 파일 확장자 | `.bin`만 허용 TODO |
| 파티션 크기 | OTA slot에 맞는지 확인 |
| 보드 호환성 | AtomS3-Lite/Atom-Lite mismatch 방지 TODO |
| 인증 | Basic Auth 적용 |

## TODO

- ElegantOTA 실제 endpoint 경로 확인
- 보드별 firmware artifact naming 규칙 확정
- 실패 시 rollback 메시지와 LED 패턴 확정
