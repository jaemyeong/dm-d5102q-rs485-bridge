# 기여 가이드
> 🌐 Read this in: **한국어** · [English](./CONTRIBUTING.en.md)

## 개발 환경

- Arduino IDE 2.x
- ESP32 보드 매니저 URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
- 권장 라이브러리: `M5Atom`, `M5AtomS3`, `ESPAsyncWebServer`(ESP32Async 포크), `AsyncTCP`, `Preferences`, `ElegantOTA`, `ArduinoJson`
- LittleFS uploader 플러그인: 보드별 `data/` 업로드 검증에 사용

## 브랜치 전략

- 기능: `feat/<topic>`
- 버그 수정: `fix/<topic>`
- 문서: `docs/<topic>`
- 유지보수: `chore/<topic>`

## 커밋 규칙

Conventional Commits 1.0.0을 사용합니다.

예시:

```text
feat: add packet framing strategy
fix: handle tcp reconnect backoff
docs: update baud scanner guide
```

## shared 동기화 규칙

`shared/`가 마스터입니다. `shared/data/*`와 `shared/src/*`는 `tools/sync_shared.sh`로 `firmware/atoms3-lite/`와 `firmware/atom-lite/`에 단방향 복사합니다.

금지 사항:

- `shared/src/*` 내부 보드별 `#ifdef`
- `firmware/*/src/*` 직접 편집
- `<board>.ino`, `board_config.h`, `secrets.h.example`, `partitions.csv`의 자동 복사

## 코드 스타일

- C++/Arduino: 4-space indent
- Web/YAML/JSON: 2-space indent
- Python tools/tests: PEP 8, `ruff`, `black --check`
- Markdown: 한글 문서를 기준으로 작성한 뒤 `.en.md` 영어 보조 문서를 함께 갱신

## PR 체크리스트

- [ ] 양쪽 보드(AtomS3-Lite, Atom-Lite) 컴파일 확인
- [ ] `shared/` 변경 시 동기화 실행
- [ ] 관련 문서 한·영 페어 갱신
- [ ] 사용자 소유 장비 디버깅 범위를 벗어나지 않는지 확인
- [ ] 보안 영향이 있으면 `SECURITY.md` 기준으로 설명

## 이슈 신고

버그, 기능 제안, 하드웨어 질문, PRD 변경 제안은 GitHub Issue Template을 사용해 주세요. 보안 취약점은 공개 이슈에 올리지 말고 `jaemyeong@me.com`으로 신고해 주세요.
