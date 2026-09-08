# B1 signed OTA bootstrap

## Current evidence, 2026-09-09

Installed firmware is now **usb-bootstrap-0.2.1**, OTA version **201**. After
the verified 0.2.0 USB baseline, the user's `다음` continued one normal OTA bench
test. Signed transfer, healthy PENDING_VERIFY, automatic authenticated confirm
and independent VALID all passed. Existing Wi-Fi/key and schema/revision 1
were retained; both numeric-IP and mDNS authenticated status passed again.
Access: `http://192.168.1.55` or `http://dm-bridge-8810a1.local`.
See the [first OTA receipt](../../.arduino/evidence/upload/signed-ota-bench-201-20260909.md)
and [candidate build](../../.arduino/evidence/build/signed-ota-bench-201-20260909.md).
One image was uploaded once; no USB port opening, fault injection or watcher
activation. Opposite-slot OTA and failed-update rollback remain untested.

The preserved **usb-bootstrap-0.2.0**, OTA version **200**, is the host-tested,
signed, USB-installed baseline from which the first normal OTA succeeded:

- Bundle: `.arduino/build/usb-bootstrap-0.2.0-195ad1aa6f44/`
- Application: 832080 bytes; SHA-256
  `195ad1aa6f44222786ae749afe1ef7eabf76b461037e2d142fa1582724bfca63`.
- Unchanged 1310720-byte image ceiling: 478640 bytes remaining. Static RAM:
  54720 bytes. Two post-upload free-heap samples were 186836/186224 bytes with
  a 110580-byte largest block; loaded/soak heap, stack, flash timing and
  watchdog failure behavior remain unmeasured.
- 98 Python tests pass, including 2526 core, 221 transport/runtime, 145 browser
  and 485 OTA assertions, sanitizers and independent crypto interoperability.
- [Build receipt](../../.arduino/evidence/build/signed-ota-bootstrap-20260909.md).

The user canceled the proposed button-only policy. After saving a new Wi-Fi
trial, the ATOM has 60 seconds to establish that connection; failure withdraws
the trial and returns to protected, untimed setup AP. It is **not a deadline
for visiting a web page**. Established-network outages retain settings and
retry without opening AP. The existing power-on button hold resets Wi-Fi only;
installation credentials and OTA trust are not reset. No extra GPIO or RS485
driver was added. TX remains prohibited.

## Signed image and receiver

The device trusts a compiled Ed25519 public key, not a key supplied by the
upload. A manifest is exactly 116 bytes, followed by its 64-byte signature:

| Offset | Bytes | Encoding |
|---|---:|---|
| 0 | 8 | `DMOTA1\r\n` |
| 8 | 4 | Monotonically increasing uint32 version, little endian |
| 12 | 4 | Exact application length, little endian |
| 16 | 4 | Minimum supported configuration schema, little endian |
| 20 | 16 | `m5stack-atom`, NUL and zero padding |
| 36 | 48 | Build ID, NUL and zero padding; ASCII letters/digits/`_.-` |
| 84 | 32 | SHA-256 of the application |
| 116 | 64 | Standard Ed25519 signature over the preceding 116 bytes |

Firmware embeds its build/version release tag. Packaging checks that tag and
the actual public key in the image. Board mismatch, unsupported schema,
non-increasing version, invalid signature, malformed fields, oversize image
and final image-hash mismatch fail closed. The USB baseline and ordinary OTA
are separate mechanisms; there is no remote downgrade-bypass endpoint.

| Route | Gate and action |
|---|---|
| `GET /api/v1/ota/status` | Digest authentication; actual build/device/heap/update/boot state |
| `POST /api/v1/ota/prepare` | Digest, same-origin and boot-CSRF; verify 180-byte envelope; issue one-use transaction token |
| `POST /api/v1/ota/upload` | Same gates plus token and exact length before reading image body or beginning flash writes |
| `POST /api/v1/ota/confirm` | Same gates plus boot transaction and local health; confirm pending image |

Prepare expires after 30 seconds. Upload chunks are bounded to 1024 bytes,
with an 8-second idle and 120-second absolute deadline. The receiver writes
only the inactive application slot. It verifies the complete image and saves
a CRC-protected transaction journal in `dmboot` NVS before selecting that slot.
Interrupted/incomplete uploads do not select a new boot slot. OTA does not
update the bootloader, partition table, filesystem or enrollment/Wi-Fi records.

The pinned ESP32 SDK enables rollback. The strong `verifyRollbackLater` override
prevents Arduino's early automatic confirmation. An OTA-pending boot must
match the durable journal, satisfy local configuration/Wi-Fi/server/heap health
and receive an authenticated controller acknowledgement within **30 seconds of
startup**. A 10-second task watchdog covers a stalled application loop. The
30-second OTA confirmation is automatic controller work, not a user browser
deadline. Rejection or timeout requests bootloader rollback. Source, linkage
and host fakes do **not** prove real power-loss or bootloader recovery; that
requires the bench sequence below. See
[Espressif's pinned-version rollback contract](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/ota.html#app-rollback).

## Trust material and tooling

The original device installation key is unchanged. It was read privately into
local client memory for GET-only baseline checks and the subsequent authenticated
OTA transaction; it was not printed, passed as an argument or rewritten.
New local signing material was generated in the ignored 0700 directory
`.arduino/evidence/hardware/private/b1-signing-20260909/`; `signing-key.pem` and
`public-key.der` are 0600. The ignored public-only compiler header is
`.arduino/tmp/ota_identity.h`. Never print, commit, replace or transmit the
private signing key. Arrange its private backup before relying on unattended
development; a repository copy alone does not contain it. A build without a
configured public key disables OTA, and host fixture keys cannot compile as a
device build.

Public key ID (SHA-256 of raw public key):
`1950c9592a94a88c2cb7184e8b2a841ae75d25003633a0c940d51b7100c73d51`.

Verification uses vendored Monocypher 4.0.3, commit
`ab2b16dd619ad5f6979a4fbe69cfa324a6fcc35f`.
No package/framework upgrade was introduced. Standard Ed25519 is tested against independent OpenSSL signing
and verification and the native device verifier. Source notices and the BSD
2-Clause license are retained. References:
[Monocypher 4.0.3 release](https://github.com/LoupVaillant/Monocypher/releases/tag/4.0.3),
[Ed25519 API](https://monocypher.org/manual/ed25519),
[OpenSSL pkeyutl](https://docs.openssl.org/3.0/man1/openssl-pkeyutl/).

No Secure Boot, eFuse anti-rollback, flash encryption or HTTPS claim is made.
HTTP Digest authenticates management; signatures authenticate firmware.
Use the existing trusted LAN/VPN, not public port forwarding. Routed VPN may
require the numeric IP because `.local` multicast may not traverse it.

## Mac workflow after the USB baseline is approved and installed

`scripts/ota_controller.py` uses Python's standard library and local OpenSSL.
Credentials are prompted without echo unless a 0600 file containing only the
20-character installation key is supplied with `--credential-file`. Never put
the key directly on a command line. It verifies the exact device ID, board,
TX-blocked state, trust key, schema and increasing version before preparing.

For the selected device, a future status check is:

```sh
python3 scripts/ota_controller.py status \
  --url http://dm-bridge-8810a1.local --device-id dm-bridge-8810a1
```

For each subsequent build, increase both the compiled build ID and integer
OTA version using reviewed build flags or a source revision; preserve the
public-key header and pinned toolchain. The first 201 build used flags only,
so `config.h` still defaults to the sealed USB baseline 200. A plain default
build must not be mistaken for a new OTA release. The exact flag recipe is in
the candidate build receipt above.
Run the regression suite and image-budget build before packaging the matching
version/build ID. The following historical 201 example describes the interface;
201 is already installed. Do not repackage or redeploy it as a new update:

```sh
python3 scripts/ota_controller.py package \
  --image .arduino/build/platformio/atom_lite/firmware.bin \
  --key .arduino/evidence/hardware/private/b1-signing-20260909/signing-key.pem \
  --version 201 --build-id usb-bootstrap-0.2.1 \
  --output .arduino/build/dev-201
python3 scripts/ota_controller.py verify .arduino/build/dev-201
python3 scripts/ota_controller.py deploy \
  --url http://dm-bridge-8810a1.local --device-id dm-bridge-8810a1 \
  --release .arduino/build/dev-201 \
  --state-dir .arduino/evidence/hardware/private/ota-controller
```

The bench run used `deploy` with the existing private installation record in
memory and release `.arduino/build/ota-bench-0.2.1-a86b84cfdc9f/`, not the example
`dev-201` path. The canonical controller state directory below is now SUCCESS;
retain it for reconciliation and quarantine rather than starting fresh state.
The first baseline, version 200, could not be OTA-deployed to the former 0.1.2
because that firmware had no receiver; its USB installation is now complete.
The currently installed version 201 cannot accept a same/lower-version image;
the next OTA exercise needs a separately built higher version.

`watch` accepts the same arguments as `deploy` and polls the explicitly selected
local development release every 15 seconds in the foreground. It can follow an
atomically promoted `dev/current` symlink to an immutable release directory.
No background service, CI job, automatic build/test qualification or promotion
was installed or activated. Release qualification/promotion remains an explicit
operator or future pipeline step; watching does not run tests for you.

Controller intent and transaction state are saved privately before upload,
under a per-device process lock. If an upload response is lost, it polls the
actual device and confirms the exact matching healthy pending image; it never
automatically uploads those bytes again. Restart resumes reconciliation.
Failure/uncertainty quarantines that device and stops automatic updates.
Do not delete the state or blindly rerun uploads to bypass quarantine: inspect
the device, transaction and rollback outcome first. Successful confirmation
is followed by an independent `VALID` status check.

## Next gate and bench-to-field sequence

1. **Completed:** the user approved the exact 0.2.0 image above and it was
   uploaded once. Do not repeat that approval or upload. The host-only default
   `python3 scripts/upgrade_signed_ota.py` checks the sealed candidate, current
   source/public header and prior backup without opening a port. Its upload
   mode additionally requires explicit `--upload --approved-image-sha256`.
   Old 0.1.2 upload helpers intentionally reject the changed source.
2. **Completed USB step:** fresh checks identified FTDI 0403:6001 serial
   2956578B12, ESP32-PICO-D4, MAC 14:2b:2f:a1:10:88 and 4 MiB. At 115200,
   a fresh private full backup, known-baseline checks, verified segments,
   preserved NVS and build/key-reuse/STA/OTA baseline startup all passed. Do not
   access the excluded S3 device or combine USB and field power.
3. **First normal A-to-B complete:** 200 → 201 passed on the USB-powered ATOM
   with retained settings, observed pending/VALID acknowledgement and six bounded
   heap samples. No slot-number readback or both-slot claim. Next, exercise
   another higher version C to cover the other slot. Test bad signature,
   interruption, failed health/no acknowledgement and real rollback to a
   known-good **OTA-capable** image before moving the device. Fault injection
   requires its own scoped device action and recovery preparation.
4. The user confirms installed button and USB recovery access is possible;
   actual recovery is still untested. Keep the physical power/passivity gates
   separate. The latest tentative 5 V report needs its measurement/label point:
   [M5Stack specifies 9–24 V external input converted to 5 V](https://docs.m5stack.com/en/atom/tail485).
   Do not relabel the external terminal as 5 V or apply power by assumption.
5. Only after voltage/polarity/isolation and boot/reset A/B passivity evidence,
   proceed with separately approved power-only placement, then RX-only bus
   integration and selected development-channel watching. No field TX is
   authorized. Freeze a build for the later 72-hour soak; update outages are
   measurement gaps, not zero-loss evidence.

This is B1 host/build, USB baseline and one normal OTA/health-confirmation result.
M0 remains blocked, product acceptance is 0/11, and M1–M9 have not started.
No actual failed-update rollback, both-slot, VPN, Tail485 power, bus passivity
or RS485 acceptance is claimed. The short successful heap samples do not replace
loaded/long-duration measurements. Existing two
core-dump-partition boot warnings remain recorded in the upload receipt.
