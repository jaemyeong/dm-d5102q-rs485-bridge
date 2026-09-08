# DMOTA2: GitHub pull and manual web update

Status, 2026-09-09 KST: **implemented and host/build verified, not device verified**.
User approval of the [plan](github-release-ota-plan.md) authorizes this G1–G3
implementation. Candidate: `usb-bootstrap-0.3.0`, integer version `300`.
The last measured device image remains `0.2.1/201`; it was not accessed this turn.
No Release, CI secret or automated deployment service was created.

## Operation after the separate bootstrap installation

- Log in to the device dashboard on the configured LAN/VPN connection.
  Select one signed `.dmota` file, review board/version/size, then press the
  explicit update button. Raw unsigned `.bin`, ZIP and flash dumps are rejected.
- The browser sends the 192-byte header first. Only an authenticated, same-origin,
  CSRF-valid request with a valid signature receives a short-lived one-use token.
  The application body streams to the inactive slot through the existing writer.
- A lost response triggers read-only status observation, never automatic re-upload.
  Success requires the matching transaction/build/version and actual `VALID` boot
  state, not completion of browser transmission. The page has no persistent login
  storage. Closing it after a complete verified upload does not prevent the device
  from confirming its own healthy boot; closing it mid-upload is not success.
- **GitHub 확인 후 업데이트** explicitly checks and installs a newer valid Release.
  It is not a read-only preview. The action is authenticated and rate-limited.
- Periodic checking is implemented but **off** (`DM_GITHUB_AUTO_UPDATE=0`) in
  this candidate. An eventual, separately verified build with value `1` checks
  after 30–60 seconds and then every 5 minutes ±30 seconds, without the Mac.
  There is no unauthenticated toggle, background Mac watcher or hidden activation.

Manual file upload needs no GitHub connection; actual offline-LAN/iPad Safari
tests are still pending. Management HTTP has the existing Digest authentication
but is not encrypted HTTPS. Use a trusted LAN/VPN, not Internet port forwarding.
Authentication, mDNS, untimed AP, first-Wi-Fi-failure 60-second fallback and the
boot-button recovery policy are unchanged. AP onboarding does not permit OTA.

## Signed wire contract

All integers are unsigned little-endian. Strings are canonical ASCII, NUL
terminated and zero-padded. The file contains exactly this header and image.

| Offset | Bytes | Meaning |
| --- | --- | --- |
| 0 | 8 | `DMOTA2\r\n` domain |
| 8 | 4 | Monotonically increasing integer version |
| 12 | 4 | Application image size |
| 16 | 4 | Minimum configuration schema |
| 20 | 16 | Board ID: `m5stack-atom` |
| 36 | 48 | Build ID |
| 84 | 32 | Application SHA-256 |
| 116 | 4 | Minimum updater protocol: `2` |
| 120 | 8 | Signed channel: `stable` |
| 128 | 64 | Ed25519 signature of bytes 0–127 |
| 192 | image size | Application binary |

The pinned device public key is the trust anchor. Package-supplied keys, GitHub
names/tags/hashes and browser preview are not trusted replacements. The signing
tool requires the matching public key and release/updater identity tags in the
image. No new signing key is needed or generated for this migration.

```sh
python3 scripts/dmota_package.py package \
  --image /path/to/firmware.bin --key /private/path/signing-key.pem \
  --version 301 --build usb-bootstrap-0.3.1 \
  --output /private/output/dmbridge-atom-lite-301.dmota
python3 scripts/dmota_package.py verify /private/output/dmbridge-atom-lite-301.dmota \
  --public-key <previously-pinned-public-key-hex>
```

Example version 301 requires an image actually built with that version/build ID;
the tool does not relabel a version-300 image. Output creation is exclusive,
private (0600), and bounded by the 1310720-byte application limit. Keep the key
outside Git. Publish only a verified package after the relevant deployment gate.

## Boot compatibility and retry policy

The old DMOTA1 push endpoint and its 180-byte envelope remain supported. Its
external authenticated Mac ACK policy is unchanged. The existing `dmboot/ota`
record remains **152 bytes** so version 201 can still read it after rollback.

New `web-file`/`github-pull` origins use a separate **164-byte `otactx2` record**:
domain/version, old 116-byte manifest, 32-byte transaction, origin, reserved zeros
and CRC. The trusted entrypoint writes and reads back this context **before erase**.
Selection checks its binding again before writing the legacy journal. No Wi-Fi,
enrollment, bootloader, partition table, filesystem or eFuse data is updated.

The context records the attempted version/hash. The implementation conservatively
rejects **all candidate versions at or below the attempted version**, including
an interrupted or rejected attempt after authorization. A higher signed version
is required; a quarantine-clear/downgrade bypass is not implemented. Before flash
authorization, malformed/signature-invalid candidates cause no attempt write.
An invalid context disables new updates; it does not erase configuration.

Local boot confirmation requires expected image identity, valid configuration,
STA IPv4, management readiness, TX blocked, heap limits and **5 continuous seconds**
of healthy loop observations (gaps under 250 ms) within the 30-second boot deadline.
An external ACK cannot bypass this observation for new origins. GitHub Internet
reachability is not a boot-health requirement. SDK pending-image rollback and the
10-second watchdog remain; actual failed-boot/power-cut recovery is **UNV**.
There is no promise of recovery when no valid previous slot exists.

## GitHub transport and bounds

Outbound access is a narrowly approved firmware-distribution exception to the
original local-only design: fixed public GitHub repository, DNS and NTP only.
No wallpad packets, Wi-Fi credentials, telemetry or GitHub PAT are transmitted.
NTP uses `time.cloudflare.com` and `pool.ntp.org`; invalid/unavailable time defers
updates. TLS verifies CA chain and hostname using the pinned IDF 4.4.7 full CA
bundle. CA freshness and actual target handshake compatibility remain bench gates.

- Fixed `/repos/jaemyeong/dm-d5102q-rs485-bridge/releases/latest`, API `2026-03-10`.
  Require non-draft/non-prerelease and exactly one usable asset named
  `dmbridge-atom-lite-<integer-version>.dmota`; pin its numeric asset ID/size.
- Fetch that asset ID, not an arbitrary `browser_download_url`. At most three
  redirects, HTTPS only, exact hosts `api.github.com` and
  `release-assets.githubusercontent.com`. No wildcard hosts, URL credentials,
  custom ports or logged redirect query strings. The latter hostname was observed
  from an official Espressif release asset's live API redirect, not assumed.
- JSON 16 KiB, HTTP header 8 KiB, URL 1536-byte buffer, ETag 128-byte buffer,
  image chunks at most 1024 bytes. Oversize/malformed metadata fails closed.
  Content-Length or bounded chunked framing is required. The final image bytes
  are withheld from the writer until HTTP body framing/total size also pass.
- 404 means no Release; 304 needs an existing ETag. 403/429 honor retry/reset
  delays; extreme or unsupported retry values suspend further checks until reboot.
  Network/format/time failures use exponential backoff with jitter. ETag and
  periodic state stay in RAM. Failed downloads clear ETag for a future fresh check.
- A dedicated core-0 FreeRTOS worker with a static 16 KiB stack owns TLS. Static
  one-request/two-message queues and per-chunk ACKs cross to the main loop, which
  alone owns OTA/NVS/flash. All three entrypoints share its busy state.
  TLS connect timeout is 3 s, worker idle 8 s, job 180 s; the existing image writer
  also retains its 120 s receive limit. The synchronous DNS/SDK cancellation
  latency, web latency and watchdog behavior require real-target measurement.
- Authenticated status exposes result/HTTP code/next check and sampled minimum
  heap/largest block/stack headroom, not payload or secrets. IDF's stack watermark
  is already in **bytes**. Sampling and static RAM figures are not soak evidence.

Primary contracts: [GitHub Releases](https://docs.github.com/en/rest/releases/releases#get-the-latest-release),
[assets](https://docs.github.com/en/rest/releases/assets#get-a-release-asset),
[rate limits](https://docs.github.com/en/rest/using-the-rest-api/rate-limits-for-the-rest-api),
[IDF ESP-TLS](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/protocols/esp_tls.html),
[IDF rollback](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/ota.html#app-rollback).

## Verified evidence and remaining gates

Full private-workspace regression: **102 tests**, including 2526 core, 327 HTTP,
240 browser, 965 OTA, 431 GitHub parser/coordinator and 157 actual-worker-with-fake-TLS
assertions. The signing tests independently compare OpenSSL and native Monocypher.
These cover interruption models, signature/hash/framing failures, legacy journal
compatibility, local confirmation, retry policies and one-writer coordination;
they are not real Safari, TLS, FreeRTOS scheduling or physical rollback evidence.

Pinned build: **1046336-byte image**, **264384-byte margin** below 1310720;
static RAM **96016 bytes**. Image SHA-256:
`80ffa771d5907cb5321f6d35a150066ac597666f229fc81637062a5e97c45988`.
The separate local receipt retains sealed DMOTA1 bootstrap and DMOTA2 artifacts.
The fixed dual-OTA layout, toolchain and signing identity are unchanged.

Next: G4 exact version-300 installation through the old signed Mac push/ACK,
then new-version manual upload and explicit GitHub installation tests, both-slot
and actual fault recovery, offline iPad Safari and real TLS/heap/stack measurements.
Only after those pass does G6 approve automatic Release deployment. The version
201 device cannot install a `.dmota` file directly. No field wiring, bus TX or
product milestone is accepted by this implementation.
