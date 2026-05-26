# tools
> 🌐 Read this in: **한국어** · [English](./README.en.md)

PC에서 실행하는 보조 도구를 둘 위치입니다. 이 스캐폴딩 단계에서는 소스 파일을 만들지 않고, 후속 구현할 스크립트 목록과 책임만 정의합니다.

## 예정 스크립트

| 스크립트 | 목적 |
|---|---|
| `sync_shared.sh` | `shared/`를 보드별 펌웨어로 단방향 복사 |
| `bump_version.sh` | 버전 문자열과 CHANGELOG 갱신 보조 |
| `check_doc_sync.sh` | `.md` / `.en.md` 페어 갱신 확인 |
| `ota_upload.sh` | 로컬 OTA 업로드 보조 |
| `littlefs_pack.sh` | `data/`를 LittleFS 이미지로 패킹 |
| `tcp_client.py` | TCP Bridge client 모드 테스트 |
| `tcp_server.py` | TCP Bridge server 모드 테스트 |
| `ws_client.py` | WebSocket console 테스트 |
| `packet_logger.py` | RS485/TCP/WS 프레임 파일 기록 |
| `packet_replay.py` | 캡처된 HEX 프레임 재생 |
| `baud_scan_eval.py` | Baud Scanner 결과 분석 |
| `nvs_dump.py` | NVS 설정 dump 보조 |

## 작성 규칙

- 실제 스크립트는 후속 구현 단계에서 추가합니다.
- Python 도구는 `ruff`와 `black --check`를 통과해야 합니다.
- Shell 도구는 macOS 기본 bash 환경에서 동작하도록 작성합니다.
- 민감 정보가 로그에 남지 않도록 기본 출력은 보수적으로 둡니다.

## TODO

- 각 도구의 CLI 인자와 출력 포맷 확정
- 공통 테스트 fixture와 캡처 파일 포맷 연동
