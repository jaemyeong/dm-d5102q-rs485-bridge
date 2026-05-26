# tests
> 🌐 Read this in: **한국어** · [English](./README.en.md)

펌웨어 알고리즘 중 PC에서 검증 가능한 부분을 pytest로 포팅해 테스트할 위치입니다. 이 단계에서는 테스트 소스 파일을 만들지 않습니다.

## 예정 구조

| 파일 | 검증 대상 |
|---|---|
| `test_hex_codec.py` | HEX 문자열 파싱, formatting, 오류 처리 |
| `test_packet_framer.py` | idle-gap/delimiter/length 프레이밍 |
| `test_ring_buffer.py` | overflow와 wraparound 동작 |
| `test_baud_score.py` | Baud Scanner 품질 평가 |
| `test_config_validation.py` | NVS 설정 검증 규칙 |

## Fixture

`tests/fixtures/`에는 익명화된 HEX 프레임, scanner 결과, API payload 샘플을 둡니다. 실제 사용자 장비 캡처를 추가할 때는 주소, 세대 식별값, 시간 정보를 익명화합니다.

## 실행 예정 명령

```text
python -m pytest tests
ruff check tools tests
black --check tools tests
```

## TODO

- 펌웨어 C++ 알고리즘의 Python reference 구현 범위 결정
- 캡처 fixture 파일 포맷 확정
