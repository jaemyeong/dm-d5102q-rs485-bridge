# DM-D5102Q RS485-Wi-Fi Bridge

Dedicated firmware for an M5Stack Atom Lite (C008) and Tail RS485 / Tail485
(T002), selected by the user on 2026-09-07. The product goal is a local, observable RS485-to-raw-TCP/WebSocket
bridge. It must not claim Modbus compatibility or infer the proprietary
DM-D5102Q payload meaning.

## Current status

The public source repository is
[jaemyeong/dm-d5102q-rs485-bridge](https://github.com/jaemyeong/dm-d5102q-rs485-bridge).
Next planned delivery model: **device-initiated GitHub Release OTA plus manual
firmware-file upload in the web dashboard**. See the
[implementation plan](docs/cleanroom/github-release-ota-plan.md).
These new paths are not implemented or enabled yet; current B1 uses authenticated
Mac push. No Release or automated signing workflow has been published by this step.

Milestone M0 (clean-room baseline and evidence sealing) is in progress. The
historical GPIO-free heartbeat has now been replaced by standalone B0 onboarding.
The user-approved [USB-first execution amendment](docs/design/USB-BOOTSTRAP-AMENDMENT-2026-09-08.md)
now enables a separate B0 onboarding candidate while product acceptance gates
remain unchanged. [B0 firmware and test instructions](docs/cleanroom/usb-bootstrap.md)
cover protected AP web setup, validated Wi-Fi trial storage, reboot and STA.
Host tests/build, verified USB upload and private first-boot capture passed.
The [upload receipt](.arduino/evidence/upload/usb-bootstrap-upload-20260908.md)
records AP startup from serial, not real browser/STA/LAN/VPN verification.
The [0.1.2 upgrade receipt](.arduino/evidence/upload/boot-reset-mdns-upload-20260909.md)
records the subsequent credential-preserving upload with HTML login, untimed AP,
boot-button Wi-Fi recovery and per-device STA mDNS.
The user subsequently confirmed login, mDNS and configured Wi-Fi access;
the earlier AP-only observation is historical. The existing key is retained.
Actual button qualification, VPN and all Tail485/RS485 behavior remain unverified.

[B1 signed OTA](docs/cleanroom/signed-ota-bootstrap.md) is now implemented and
host-tested: 98 tests pass. After the verified 0.2.0 USB baseline, the first
signed OTA **0.2.0 → 0.2.1 passed**, including observed pending health
confirmation and `VALID` status. Installed firmware is 0.2.1/version 201,
832080 bytes; existing Wi-Fi/key and authenticated IP/mDNS access were retained.
Use the device's actual IP or its own `dm-bridge-<id>.local` name. See the local
[first OTA receipt](.arduino/evidence/upload/signed-ota-bench-201-20260909.md).
Opposite-slot updates and failed-update/rollback recovery still need bench tests
before field deployment; automatic watching remains inactive. The initial
60-second Wi-Fi connection-failure fallback remains; it is not a browser deadline.

The user reports that Atom Lite is currently connected to the development Mac
by USB. The intended field setup uses Tail485 terminal power/data, then keeps
the assembly installed for automated signed OTA development once update and
recovery behavior are verified. The
[field OTA development plan](docs/cleanroom/field-ota-development-plan.md)
records that workflow; the new Mac controller is implemented but no automatic
deployment service or watcher is active.
The selected management path supports both same-LAN and VPN access. Initial
setup uses protected AP web onboarding, saves Wi-Fi settings and reboots
into STA; ordinary OTA should retain those settings.

Safety defaults:

- Atom S3 Lite is explicitly out of scope.
- RS485 transmit is disabled until a later, separately approved milestone.
- Do not connect USB power and bus power together until the power-path gate in
  [the wiring checklist](docs/cleanroom/wiring-checklist.md) passes.
- Missing measurements are `UNV`; a build or heartbeat does not promote them
  to hardware evidence.

## Baseline artifacts

- [Approved T002 target amendment](docs/design/TARGET-AMENDMENT-T002-2026-09-07.md) — overrides the preserved v1.0 A131 hardware assumptions
- [Preserved baseline design v1.0](docs/design/DM-D5102Q-RS485-WIFI-BRIDGE-SPEC-v1.0.md)
- [M0 status and clean-room boundary](docs/cleanroom/README.md)
- [Requirement baseline](requirements/requirements.json)
- [M0-M9 detailed gate state](requirements/milestone-state.json)
- [Durable hardware-loop contract](requirements/project-loop-contract.json)
- [Flash and runtime memory budget](requirements/memory-budget.json)
- [Dual-OTA partition table](partitions/dmbridge_ota.csv)
- [Initial test vectors](tests/vectors/initial.json)
- [Evidence register](docs/cleanroom/evidence-register.json)
- [Locked public hardware sources and unresolved conflicts](docs/cleanroom/public-source-lock.json)
- [Physical evidence packet and RX-only capture contract](docs/cleanroom/physical-evidence-contract.md)
- [Threat model](docs/cleanroom/threat-model.md)
- [Toolchain lock](docs/cleanroom/toolchain-lock.md)
- [Durable project goal](.arduino/goal.md)
- [One next physical gate](.arduino/next-todo.md)

## Public clone versus private bench workspace

Source, tests, design documents and third-party license notices are published.
The entire `.arduino/` evidence/state/build tree, agent caches, installation and
signing keys, flash backups, raw photos/logs and packaged firmware remain local.
Links into `.arduino/` refer to that private bench workspace and are intentionally
not included in a public clone. No private key is required to read public Releases.

A fresh clone can run the standalone signing/controller tests with Python 3,
OpenSSL 3 and C/C++ compilers:

```sh
python3 -m unittest discover -s tests -p 'test_ota_controller.py' -v
```

The full 98-test local result includes sealed bench-artifact/evidence checks;
do not expect a public clone lacking private artifacts to reproduce that result.
Never download or fabricate private backups just to make those gates pass.
The target build also requires the pinned toolchain and a deliberately supplied
public verification-key header; absent that header the firmware disables OTA.
Device-specific upload helpers contain historical exact-target/image guards and
must not be used as generic upload commands or to reflash an already-updated unit.

In the original private bench workspace, run the host-only M0 consistency check with:

```sh
python3 scripts/validate_m0.py
python3 scripts/validate_milestone_gates.py --report
python3 scripts/run_arduino_evals.py
python3 scripts/validate_memory_budget.py
python3 scripts/validate_public_sources.py
python3 scripts/validate_evidence_packet.py
```

Build the current USB-bootstrap/signed-OTA candidate through the single pinned
environment with (Tail485/RS485 application I/O remains absent):

```sh
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
  .platformio-venv/bin/pio run --environment atom_lite
```

Run `python3 -m unittest discover -s tests` for the regression suite, including
C++ sanitizers, storage/network failure fakes and independent loopback HTTP
Digest tests. The old `examples/atom_lite_cli_smoke` source is preserved as a
historical fixture, not the active build source. Do not run an upload before
the exact-device and private first-boot key-handoff gate in the B0 instructions.

The build uses release LTO, the fixed partition table, warnings as errors, and
an automatic post-build 1.25 MiB image gate. If the package registry is not
reachable, the narrowly scoped local override format is documented in
[`platformio-local.ini.example`](docs/cleanroom/platformio-local.ini.example).

The commands validate document hashes, all 207 design-derived milestone/test/
release criteria and their state transitions, the single physical next action,
sealed and hash-linked experiment history, rollback-image integrity, evidence
proof-stage directories, requirement-to-test traceability, the evidence schema,
locked public-source hashes and unresolved conflicts, the
physical evidence/capture contract, append-only experiment ledger, fixed 4 MiB
dual-OTA layout, and single open next action. Add `--require-archives` to the public-source validator to
verify all ten locally ignored snapshots (four active C008/T002 references;
A131 references are retained as history). Pass `--image firmware.bin` to the
memory validator to enforce the 1.25 MiB application-image limit. Runtime heap,
selected UART pins, T002 revision and logic levels, and boot-time bus passivity still require
explicit physical evidence; none of these commands proves a field milestone.
