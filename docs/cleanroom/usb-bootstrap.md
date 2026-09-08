# B0 USB onboarding candidate

Current source has advanced to [B1 signed OTA](signed-ota-bootstrap.md).
This document records B0 behavior and its historical build/upload evidence;
its no-OTA descriptions apply to historical 0.1.2, not the B1 source/current image.
The user reported login, mDNS and configured Wi-Fi working on 0.1.2. B1 0.2.0
is now USB-uploaded with preserved settings and authenticated IP/mDNS status;
see the [USB receipt](../../.arduino/evidence/upload/signed-ota-bootstrap-upload-20260909.md).
The following first normal OTA to 0.2.1/201 also passed healthy pending-image
confirmation, independent VALID and retained authenticated IP/mDNS management;
see the [OTA receipt](../../.arduino/evidence/upload/signed-ota-bench-201-20260909.md).
Failed-update recovery and opposite-slot trials remain unverified.
The old upgrade helper deliberately rejects current source
drift; the exact new candidate uses `scripts/upgrade_signed_ota.py` after its
own image-specific approval, now exercised once. Do not rerun either uploader
after success. The 60-second initial Wi-Fi trial fallback stays.

This is the first standalone Atom Lite C008 application, enabled by
[USB-BOOTSTRAP-20260908](../design/USB-BOOTSTRAP-AMENDMENT-2026-09-08.md).
It is not a field/RS485/OTA release. Historical `usb-bootstrap-0.1.0` was USB-uploaded
on 2026-09-08 with device verification and private first-boot capture; see the
[upload receipt](../../.arduino/evidence/upload/usb-bootstrap-upload-20260908.md).
AP startup is serial-observed; browser/STA/LAN/VPN are still unverified.
The historical host candidate `usb-bootstrap-0.1.1` implemented the user-approved
[HTML login and unlimited AP amendment](../design/WEB-LOGIN-AP-AMENDMENT-2026-09-09.md).
It was not uploaded separately. The combined `usb-bootstrap-0.1.2` adds the
user-approved [boot-button reset and mDNS](../design/BOOT-RESET-MDNS-AMENDMENT-2026-09-09.md).
The first-install CLI remains pinned to 0.1.0; the separate
`scripts/upgrade_usb_bootstrap.py` uses the shared safe USB engine with explicit
0.1.2 pins and existing-installation acceptance. Its default is host-only.

## Implemented behavior

No configured Wi-Fi -> WPA2 AP without elapsed-time expiry -> HTML login -> trial
configuration saved -> explicit response -> reboot -> STA connection -> five
seconds with connected IPv4 -> active configuration. A sixty-second initial
connection failure withdraws the trial and returns to the same protected,
untimed AP. Save/reboot, faults or power loss can still end AP operation.

A normal STA outage preserves active credentials and retries; it does not
open provisioning. The AP does not auto-open a captive browser: use its printed
numeric URL. After STA connection, use `STA_IP` from USB diagnostics or the
router DHCP table. Existing LAN/VPN routing is reused, not configured by the
device. Since 0.1.2, same-LAN mDNS provides `http://dm-bridge-XXXXXX.local`
(lowercase hex); this selected device is `http://dm-bridge-8810a1.local`.
The responder runs only on connected STA and advertises _http._tcp port 80.
Failure leaves IP HTTP usable and is retried at most once per 30 seconds.
Routed VPN/subnet/client-isolation may block multicast: retain IP/private DNS
access there. No IPv6 HTTP access or Arduino OTA advertisement is provided.

| Interface | Behavior |
|---|---|
| AP SSID | `DM-BRIDGE-XXXXXX`, one associated client, no elapsed-time limit |
| AP and admin key | Per-device 20-character random installation key, 100 bits |
| HTTP login | HTML form; username `installer`, installation key, browser-generated Digest SHA-256 |
| `GET /` | Public static Korean login shell; no current state/secrets, no CDN |
| `GET /api/v1/auth` | Public Digest challenge metadata only, no native browser popup |
| `GET /api/v1/status` | Authenticated actual mode/IP/connection, build/revision, boot CSRF; `apTimeoutEnabled:false`, `apRemainingMs:null` |
| `PUT /api/v1/config` | Initial AP only; JSON `ssid`, `password`, `configRevision`; save and reboot |
| Other routes | Rejected; no OTA, raw TCP, RS485, scanner, HTTP reset, or established-device config writes |

The HTTP implementation has a 2048-byte header ceiling, 1024-byte body ceiling,
one serviced client, 256-byte per-loop I/O budget, five-second request and
response deadlines, 32 connections per ten seconds, and five bad-credential
attempts per ten seconds. No chunked requests, duplicate framing headers or
`Expect` uploads are accepted. Authentication, local IP/exact STA .local Host validation and
same-origin/boot-CSRF checks occur before application body reads or storage.
Nonce replay history is bounded; a failed/stale challenge rotates the nonce.
The five-minute nonce lifetime is not an AP or login-session deadline. The
browser fetches current challenge metadata before each signed operation, keeps
HA1 only in page memory, and clears it on logout/pagehide. It never retries a
configuration PUT automatically. An uncertain save requires observation.

The browser hash dependency is community-authored **js-sha256 0.11.1**, not an
M5Stack/Arduino/Espressif component. Its MIT license and exact upstream source
are embedded locally in `web_sha256.inc`; see the
[dependency record](web-login-dependency.md). `getRandomValues` is required for
cnonce, but `crypto.subtle.digest` and HTTPS-only APIs are not used. The key is
not submitted as a plaintext login form, stored in browser storage or URLs.
No TLS protection is created by moving Digest computation into JavaScript.

Digest does not encrypt HTTP content; use this interface only on the protected
AP, trusted LAN or existing VPN. See [RFC 7616 security considerations](https://www.rfc-editor.org/rfc/rfc7616.html#section-5).
No TLS, NVS encryption, secure boot or signed OTA claim is made. Flash contains
credentials: keep the unit, any future flash backup and installation key private.

## Storage and failure boundary

The isolated `dmboot` schema is implemented in `core.cpp`. CRC-protected
112-byte records and 13-byte slot/revision/CRC references prevent uncommitted
records from becoming active. Raw NVS return codes distinguish missing keys
from I/O errors. Every write is committed and read back. NVS still needs real
power-cut validation; host fault injection is not flash-atomicity proof.
The [pinned ESP-IDF 4.4.7 NVS documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/storage/nvs_flash.html)
is the underlying storage reference.

If promotion is durable but deleting the identical trial reference fails, the
active reference remains authoritative at the next boot. If a write/read-back
fails after data persisted, the HTTP outcome is unknown; firmware stops rather
than acknowledge success. A persisted trial may be retried after a separately
authorized reboot, but is never treated as already proven active.

Fault/corrupt storage disables networking and reports a fixed USB diagnostic;
there is no silent NVS reset. In 0.1.2, hold the front button while applying
power and continue for approximately five seconds (application qualification
is three seconds). This clears Wi-Fi settings/revision, keeps the existing AP/
installation key and opens the same protected AP for onboarding again. Only
GPIO39 INPUT is used. Any release before qualification cancels; a late press,
software reset, watchdog or brownout does not arm. EN hard reset may also count
as power-on. Normal uploads must keep the front button released.

The versioned reset-intent sentinel is committed/read back before deleting only
active/trial/cfgA/cfgB, and erased last. Valid pending intent resumes before
credential load. Invalid intent, missing/corrupt installation key or I/O errors
fail closed. This is not forensic flash erasure or key replacement. Future
firmware must explicitly migrate this namespace; field recovery still needs
physical testing and an independently usable management/recovery route.

Application HTTP/config/state storage is fixed-size. Core Wi-Fi/socket/NVS
initialization still uses framework internals; synchronous Wi-Fi begin, NVS
commit and USB diagnostics have not been measured on target. Do not claim
real-time loop latency or runtime heap limits from host tests/linker figures.

## Host verification

### Dashboard capacity at 0.1.1

The C008 has 4 MiB flash and 520 KB family SRAM per
[M5Stack's specifications](https://docs.m5stack.com/en/core/ATOM%20Lite).
The approved layout reserves two 1.5 MiB application slots for future OTA;
the project further caps each firmware image at 1.25 MiB (1310720 bytes).
The 0.1.1 image is 779024 bytes, leaving **531696 bytes (519.23 KiB)** for
additional UI and firmware combined under that ceiling. This is not a
dedicated dashboard reservation: later network/OTA/RS485 features use it too.
The whole static HTML/CSS/JS page is 19690 bytes plus a terminating NUL,
stored in mapped flash. Static linker RAM remains 51264 bytes; actual heap
and stack margins require device measurements, not subtraction from SRAM.

The 962560-byte unallocated flash reserve (940 KiB) is not an existing web
filesystem. Using it would need a separate layout/budget/OTA review, not a
silent allocation during this login change. Keep assets small and local;
charts/DOM rendering run on the iPad/PC while ATOM serves bounded data.

```sh
python3 -m unittest discover -s tests
python3 scripts/validate_m0.py
python3 scripts/run_arduino_evals.py
python3 scripts/validate_usb_bootstrap.py
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
  .platformio-venv/bin/pio run --environment atom_lite
```

The C++ core and actual `main.cpp` adapter run with ASan/UBSan and deterministic
storage/clock/Wi-Fi fakes. Socket-pair tests count application reads to establish
the first-body-byte gate, exercise fragmented save/reboot and real response
framing. Independent `curl --digest` against a loopback-only fake device checks
SHA-256 challenge/status/save interoperability. JavaScript syntax is checked
with Node. The compiled HTML served by the actual adapter also runs in a Node
VM with DOM fakes, no Node crypto backend inside the browser VM, SHA-256 vectors,
real loopback Digest authentication/save, incorrect key, replay/nonce rotation,
logout, unsupported crypto, rate-limit/downgrade rejection and ambiguous-save
tests. Clock fakes cover ten minutes, one day and timer wrap without AP expiry,
and retain failed-trial recovery and configured STA outage behavior.
These tests are not a real Safari/WebKit rendering or iPad observation.
Native tests require Clang/GCC, curl and Node; Linux also needs
OpenSSL development headers/library. Tests contain only synthetic credentials.

## USB upload and target verification

The user approved the combined 0.1.2 upload. Do not expect `INSTALL_KEY_ONCE`
from an existing installation or erase NVS to force it. The 0.1.2 entry point is
`scripts/upgrade_usb_bootstrap.py`; it verifies the known installed 0.1.0 image
and fixed layout in the freshly backed-up flash before any write, pins the
new bundle, and requires key reuse/no spontaneous reset on first boot. The
first-install helper below still defaults to old-image/first-key behavior and
must not be rerun. A successful upgrade is not permission to repeat it.

Before an image-specific USB upload: freshly match `0403:6001`, serial
`2956578B12`, exact C008 chip/4 MiB flash; exclude the S3 port/device. Keep
Tail485 detached. No 12 V or field wiring change is part of this step. Keep the
historical full-flash backup private and unchanged; no full erase is implied.

Arrange a private 115200-baud first-boot serial capture **before releasing the
new application from reset**. The installation key is printed only when it is
first created, not on each boot. An ordinary upload-and-reboot followed by a
late monitor can miss it; do not use that sequence. The reviewed helper below
coordinates post-flash reset and credential handoff. Do not echo the key to chat,
commit it, or save it in ordinary build logs. Lost-key recovery is not in B0.

The image-specific helper defaults to host-only discovery and pinned-tool/hash
checks, without opening a serial port:

```sh
.platformio-venv/bin/python scripts/upload_usb_bootstrap.py
```

Only with the existing image-specific approval, add `--upload`. This opens
exactly the approved FTDI target, checks the known chip/revision/MAC and 4 MiB
flash, reads a new full-flash backup at 115200 baud, verifies its device MD5,
and then writes only the four locked image segments. It verifies all segments
and preserved NVS regions before resetting into the application on the same
open serial connection. There is no full erase, automatic restore or retry.
Do not rerun it after a successful first boot as an onboarding/recovery shortcut.

Per-run raw backup, upload log, first-boot serial capture, redacted log, report
and `credentials.txt` stay under a unique ignored
`.arduino/evidence/hardware/private/b0-usb-20260908-*` directory. The directory
is mode 0700 and files mode 0600. The installation key never goes to ordinary
command output; only capture success and AP metadata do. An incomplete first
boot stops the workflow and preserves evidence for diagnosis, without erasing
NVS or replacing the key.

After the separately approved upgrade, observe real AP creation and WPA2 login,
the HTML login on the actual iPad mini Safari, correct
and incorrect Wi-Fi setup, reboot, DHCP address, LAN and VPN status access,
power-cycle persistence, AP availability beyond ten minutes and STA outage behavior. Record browser
name/version, image hash, timing and redacted outcomes separately. Host fakes
do not establish any of these target observations.

Field power/wiring/Tail485 gates remain open. Only after B0 target verification
should B1 implement signed OTA with actual bootloader rollback and recovery.
