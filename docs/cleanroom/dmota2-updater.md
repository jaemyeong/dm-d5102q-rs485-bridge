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
  The 303 candidate assigns TLS connection setup at most 15 s, capped by the
  remaining job budget. Worker idle stays 8 s, job 180 s; the existing image writer
  also retains its 120 s receive limit. The synchronous DNS/SDK cancellation
  latency, web latency and watchdog behavior require real-target measurement.

### TLS diagnostics (302 bench candidate)

The authenticated status endpoints now include six scalar fields for the last
TLS connection in a **completed** GitHub job. During a running job they still
describe the preceding completed job, not live handshake progress:

| Field | Meaning |
| --- | --- |
| `githubTlsAttempted` | Connection setup was attempted, including allocation failure |
| `githubTlsConnectResult` | ESP-TLS result: 1 connected, 0 timeout/in progress, -1 failed; meaningful only if attempted |
| `githubTlsConnectMs` | Unsigned elapsed setup/handshake milliseconds, including clock wrap |
| `githubTlsEspError` | ESP-IDF error captured before the TLS handle is destroyed; 0 means none captured |
| `githubTlsError` | Underlying TLS library error code, or 0 |
| `githubTlsVerifyFlags` | Certificate-verification flags, or 0 |

Allocation failure reports `ESP_ERR_NO_MEM`. A new connection or job resets the
diagnostics; asset/redirect connections replace the preceding metadata connection.
No credentials, certificate content, response headers or signed redirect URLs
are included. These fields do not change trust, connection timeouts, retries,
signature checks, boot policy or automatic-polling activation.

The first standalone301 GitHub bench attempt on 2026-09-09 returned
`TIME_UNAVAILABLE`; one later explicit check returned `TLS_OR_NETWORK_FAILED`
with HTTP status0, without any firmware transfer. The installed image remained
healthy301. This does **not** establish whether the cause is DNS, connection
timeout or certificate validation. The 3-second SDK limit is a hypothesis only;
302 is a diagnostic candidate, not a verified TLS fix. Existing heap minima are
samples outside the synchronous handshake and can miss its lowest free heap.
Host fake tests and a successful build do not close the actual GitHub TLS,
download, recovery or automatic-activation gates.

### Bounded connection-budget candidate (303)

The subsequent 302 bench recheck reached ESP-TLS but returned SDK `0x8006`
(`ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT`) after 5676 ms, without an HTTP response.
It remained healthy `VALID`; certificate validation success was not established.
303 changes only the connection-budget policy: 3000 to at most 15000 ms, reduced
to the remaining 180-second job budget for each connection, including redirects.
Expired/cancelled jobs do not start another connection; a connection that returns
after the global deadline is rejected before sending HTTP. Millisecond subtraction
remains unsigned and tested across wrap. CA bundle, exact hostname/SNI, signed
package validation, backoff, idle/writer limits, boot policy, and automatic-OFF
default are unchanged.

This is a candidate, not a proven TLS fix or a hard wall-clock cancellation bound:
the [pinned SDK synchronous loop](https://github.com/espressif/esp-idf/blob/v4.4.7/components/esp-tls/esp_tls.c#L461)
checks elapsed time after a pending low-level step. DNS/SDK calls can overrun and
cannot be interrupted by this budget alone. Target TLS result, heap/stack, web
responsiveness and recovery must be measured before Release/automatic activation.

### Bounded resource sampling candidate (311, host-only)

310 reached the receiver's absolute120s transfer limit. Host operation counts
showed over3million heap API calls for a1048000-byte image: rawByte called alive
even for every already-buffered byte.311 keeps cancellation/job checks per byte,
but samples heap at TLS read boundaries (before and after every result, including
WANT_READ/WANT_WRITE), existing control/ACK-wait checks, and before publishing a
Header/Chunk to the writer. The loop's independent health gate remains intact.
No buffer size, heap threshold, TLS setting, idle/transfer/job deadline changes.

At most1024 raw bytes may be parsed between samples, without allocation or flash
write in that parsing path. This is a byte bound, not a wall-clock sampling SLA:
preemption can lengthen the interval. Transient dips that recover between samples
may be missed. Sampled minima are not directly comparable to prior per-byte
minima as equivalent-resolution evidence. Cancellation/job checks remain cheap
and per-byte; low resources are rejected before another TLS read or publication.
Pending ACK polling retains resource checks. SDK blocking overruns and cross-core
races are not eliminated, and a host count reduction is not a device speedup.

Host tests cover buffered cancellation/deadline and bounded-copy heap deferral,
low heap before/after TLS, WANT_READ, expiry across wraparound, pre-publication
rejection and heap/cancel/deadline during ACK wait. Existing framing/signature/
full-image/receiver deadline tests remain. Target throughput/OTA success require
a separately scoped receiver311 and higher-candidate trial; no deployment here.

### Phase-local scratch candidate (309, historical host result)

The308 trial's first decision recorded heap78848 below81920, healthFailed64,
and a negative Chunk ACK. The allocation source of that transient dip is not
identified.309 reduces a known permanent cost: Worker previously reserved a
16385-byte parsing buffer even while receiving firmware image chunks.

Worker now owns one RAII scratch allocation per parsing phase: at most8193 bytes
for request/HTTP headers, at most16385 for release JSON. Header scratch ends when
open returns; JSON scratch ends before asset download. No scratch remains at
Header/Chunk/Done exchange, and the buffers never overlap. Allocation failure
returns RESOURCE_LIMIT before image begin; every return frees scratch. JSON16KiB,
headers8KiB, per-line limits and image-chunk1024 limits are unchanged. Metadata
peak still needs its full buffer plus allocator overhead; this is not a universal
peak-memory reduction claim. No task-stack/TLS configuration/health threshold or
timeout changes, and no status polling suppression.

Pinned build static RAM drops96288->79904 bytes (16384 saved); Worker symbol
38824->22440. Host fakes verify lifetimes, allocation failures at each phase,
maximum valid JSON/header sizes and existing error paths. Target free heap,
largest block/fragmentation and GitHub completion remain unverified until a
separate receiver309 and higher candidate310 trial.

### Rejection diagnostics candidate (307) details

The authenticated status includes `githubRejection`, a loop-owned first negative
ACK snapshot retained through Done and GETs, cleared on the next successfully
started job (or reboot). Fields: present, kind (0 Header/1 Chunk), sequence,
received image bytes, messageAge/jobAge in ms, gates, heap/block, healthFailed,
and an updater-owned fixed reason label. Gate bits1/2/4/8 mean message idle,
job timeout, health false, runtime not eligible. Zero gates with rejection means
Header validation/start or Chunk writer processing rejected; inspect reason.
HealthFailed bits1/2/4/8/16/32/64/128/256 mean application/config/station/Wi-Fi/
IP/server/heap/block/pending reboot. Heap/block and mask are the same loop input
sample used for the decision, taken before dequeue; not an atomic cross-core
system snapshot. Heap81920/block32768 and worker heap65536 limits are unchanged.

Completed worker result adds githubExchange (0 none,1 queue send,2 negative ACK,
3 ACK timeout,4 cancellation,5 job timeout,6 resources), githubExchangeSequence,
githubExchangeWaitMs, and githubExchangeKind (0 none,1 Header,2 Chunk). The first
failed exchange is retained; worker result resets on execute and is copied to
the loop only at Done. During a new job these completed-result fields may still
describe the prior job, while githubRejection belongs to the current job.
No shared mutable diagnostic buffers, secret material or new NVS writes.

Host tests cover injected failures, first-record retention/reset, actual health
predicate threshold edges, authenticated-only serialization and scalar width.
These are diagnostic fields, not a fix or proof of the unique306 target cause.
Candidate307 is not deployed by these tests; future trial requires explicit scope.

### Cross-core receipt timestamp candidate (305) details

The 304 bench pull reached HTTP200 and RECEIVING, then UPDATE_REJECTED with
UPLOAD_INTERRUPTED; installed303 stayed VALID. Host regression reproduces one
concrete defect consistent with that symptom: the loop caches `now` before the
worker enqueues a message on the other core. `now - message.created` can then
underflow even when the message is only one millisecond newer, falsely rejecting
fresh Header/Chunk messages as idle-expired.

`Port::take` now returns a monotonic receipt timestamp sampled **after** dequeue;
the coordinator uses it for message/job age, writer progress and scheduling.
It does not substitute producer timestamps or relax timeout/health/signature
checks. Deterministic tests cover fresh Header/Chunk across clock wrap, actual
idle/job expiry, health failure, write failure, ACK absence and the real worker
adapter's clock sampling order. Tests are not concurrent RTOS or real TLS proof.
This establishes a host defect and fix, not the unique cause of the bench failure.
Install a separately approved signed305 via manual OTA before a later version's
GitHub-pull retest; preserve attempt304 and automaticOFF. No target validation of
this candidate or automatic-activation/recovery approval is implied.

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
