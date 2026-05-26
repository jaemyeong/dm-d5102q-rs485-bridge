# tools
> 🌐 Read this in: [한국어](./README.md) · **English**

This directory is for helper tools that run on a PC. The scaffolding phase does not add source files; it only defines the planned script list and responsibilities.

## Planned Scripts

| Script | Purpose |
|---|---|
| `sync_shared.sh` | Copy `shared/` one-way into board firmware trees |
| `bump_version.sh` | Help update version strings and CHANGELOG |
| `check_doc_sync.sh` | Check `.md` / `.en.md` documentation pairs |
| `ota_upload.sh` | Help local OTA uploads |
| `littlefs_pack.sh` | Pack `data/` into a LittleFS image |
| `tcp_client.py` | Test TCP Bridge client mode |
| `tcp_server.py` | Test TCP Bridge server mode |
| `ws_client.py` | Test the WebSocket console |
| `packet_logger.py` | Record RS485/TCP/WS frames to files |
| `packet_replay.py` | Replay captured HEX frames |
| `baud_scan_eval.py` | Analyze baud scanner results |
| `nvs_dump.py` | Help dump NVS settings |

## Rules

- Actual scripts will be added in later implementation phases.
- Python tools must pass `ruff` and `black --check`.
- Shell tools should work in the default macOS bash environment.
- Default output must avoid leaking sensitive data.

## TODO

- Finalize CLI arguments and output formats for each tool.
- Connect shared test fixtures with capture file formats.
