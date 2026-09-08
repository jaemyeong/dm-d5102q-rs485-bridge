# Toolchain and dependency lock

Status: pinned production candidate; host compile proof passed.

## Production candidate

| Input | Exact value | Provenance |
|---|---|---|
| Host | macOS 26.6.2, arm64 | current host observation |
| PlatformIO Core | `6.1.19` | PyPI package metadata checked 2026-09-01 |
| Platform | `platformio/platform-espressif32@7.0.0` | immutable commit `76b0e9b942330979860e3bc7fd3b92b383d66cc1` |
| Framework | Arduino-ESP32 `2.0.17` (ESP-IDF 4.4.7) | PlatformIO 7.0.0 release metadata |
| Board ID | `m5stack-atom` | board JSON SHA-256 `9a0750a8c6164f6751249a32b24ca8b250bc888d0345aebad5200a23b8019247` |
| Flash | 4 MiB | board definition plus target observation |
| Partition table | `partitions/dmbridge_ota.csv` | two 1.5 MiB OTA slots, 8 KiB OTA metadata, no filesystem |
| Firmware image ceiling | 1,310,720 bytes | governing design sections 6.3 and 15.1; independently checked after build |
| Build mode | release, `-flto`, `-Wall -Wextra -Werror` | committed single environment and verbose link proof |
| Upload speed | `115200` | local FTDI reliability observation; overrides board default 1500000 |
| Monitor speed | `115200` | product decision |
| Third-party libraries for M0 | none | product decision |

The exact platform commit, rather than a floating registry range, is the release
input. The first host attempt used an ephemeral `uvx` Python and stopped during
tool installation because that interpreter did not contain `pip`; no source was
compiled. A project-local Python environment then reached source compilation
and exposed a board-macro spelling difference: the M5Stack CLI core defines
`ARDUINO_M5STACK_ATOM`, while PlatformIO's board definition defines
`ARDUINO_M5Stack_ATOM`. The no-I/O fixture now accepts only those two spellings
for the same physical board. After removing an Arduino `.ino` prototype warning,
the corrected fixture passed with `-Wall -Wextra -Werror`; resolved versions,
memory use, and artifact hashes are recorded in
`.arduino/evidence/build/platformio-m0-toolchain.md`.

The registry mirror hostname was not resolvable through the host's system DNS.
The registry-provided archive was fetched using a DNS-over-HTTPS A record and
verified against its published SHA-256 before use. Clean-host registry/mirror
reproducibility remains a release gate; the package content was not replaced.
The committed configuration optionally loads
`.arduino/tmp/platformio-local.ini`, which is Git-ignored and absent on a clean
checkout. Its documented shape is
[`platformio-local.ini.example`](platformio-local.ini.example). It may override
only the package source with the exact checksum-verified archive; it does not
change the platform, framework, board, flags, partition, or source inputs.

## Partition and memory gate

[`requirements/memory-budget.json`](../../requirements/memory-budget.json)
records the fixed flash and runtime thresholds. The repository validator pins
those values independently and checks
[`partitions/dmbridge_ota.csv`](../../partitions/dmbridge_ota.csv) for exact
regions, alignment, overlap, flash bounds, two OTA slots, absence of a
filesystem, and remaining spare space.

PlatformIO derives its physical program-size capacity from the 1.5 MiB OTA
slot. The product's stricter 1.25 MiB rule is therefore enforced separately on
the serialized image:

```sh
python3 scripts/validate_memory_budget.py --image path/to/firmware.bin
```

The no-I/O fixture passed this gate with a 262,544-byte image and 1,048,176
bytes of margin. Static linker RAM use was 21,456 bytes. The clean verbose
build also proved that the final link contains `-flto` and not the framework's
default `-fno-lto`; the post-framework hook makes those flags consistent.
Neither size number is
runtime heap evidence; the 80 KiB steady, 50 KiB loaded, and 32 KiB largest
block thresholds remain `UNV` until measured on the exact target under load.

## Historical bring-up environment, not a release input

The previous smoke test used Arduino CLI `1.5.1` and M5Stack's official board
package `m5stack:esp32@3.3.9` with FQBN
`m5stack:esp32:m5stack_atom:UploadSpeed=115200,EraseFlash=none`. The board
archive checksum reported by Arduino CLI is
`4d23028594552c94ad09040c4628346c9db91374e7c1195bcb4d60d389ed6943`.
That package embeds Arduino-ESP32 3.3.9.

It remains valid evidence for the historical smoke upload, but it must not be
silently mixed with the PlatformIO/Arduino-ESP32 2.0.17 production candidate.
There will be one release environment. If the production candidate cannot meet
the security, API, partition, or memory requirements, M0 must revise this
decision explicitly and rerun all build evidence.

## Deferred dependency decisions

- The C008/T002 target amendment does not change the production toolchain.
  No operational UART GPIO constants are selected until the T002 identity and
  GPIO26/32 continuity gate passes. The official Tail485 TX26/RX32 table is
  public family evidence; A131 RX22/TX19 is not applicable. Physical pin and
  electrical behavior remain UNV.
- M1 uses Arduino core facilities only until RGB LED support is footprint- and
  API-tested; no unpinned `M5Unified` URL is accepted.
- USB-BOOTSTRAP-20260908 enables the core's pinned Wi-Fi sockets, NVS and
  mbedTLS SHA-256 facilities for B0 without third-party libraries. The bounded
  request parser authenticates headers before body reads; native fakes and
  loopback curl cover this host boundary. Full M3 networking remains unverified.
  ESPAsyncWebServer is not a dependency.
- The approved B1 software track now vendors Monocypher 4.0.3 (commit
  `ab2b16dd619ad5f6979a4fbe69cfa324a6fcc35f`) for optional standard Ed25519
  verification, under BSD-2-Clause; no on-device private signing key. Native
  negative vectors and independent OpenSSL 3.6.4 signing/verification pass.
  The upstream release fixes a signing timing issue in <=4.0.2; use the pinned
  4.0.3 files and hashes, not a floating release. This portable C99 dependency
  needs no Arduino GPIO, allocator, network or transitive library. Hardware
  timing/heap and M8 product acceptance remain unverified. ESP-IDF 4.4.7 mbedTLS
  supplies image SHA-256 but not this Ed25519 API; a full Arduino crypto stack
  was not selected. See [B1 operations](signed-ota-bootstrap.md).
- Browser assets are repository inputs and are compiled into the image; no CDN
  or remote build asset is allowed.
