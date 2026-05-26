# tests
> 🌐 Read this in: [한국어](./README.md) · **English**

This directory is for pytest coverage of firmware algorithms that can be ported and validated on a PC. No test source files are added during this scaffolding phase.

## Planned Layout

| File | Target |
|---|---|
| `test_hex_codec.py` | HEX string parsing, formatting, error handling |
| `test_packet_framer.py` | idle-gap/delimiter/length framing |
| `test_ring_buffer.py` | Overflow and wraparound behavior |
| `test_baud_score.py` | Baud scanner quality scoring |
| `test_config_validation.py` | NVS settings validation rules |

## Fixtures

`tests/fixtures/` stores anonymized HEX frames, scanner results, and API payload samples. When adding captures from real user-owned equipment, anonymize addresses, household identifiers, and timestamps.

## Planned Commands

```text
python -m pytest tests
ruff check tools tests
black --check tools tests
```

## TODO

- Decide the scope of Python reference implementations for firmware C++ algorithms.
- Finalize capture fixture file formats.
