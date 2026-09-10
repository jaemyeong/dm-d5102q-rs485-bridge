# System dashboard

Deployment update (2026-09-11 KST): signed316 was installed on dm-bridge-8810a1
through one manual-file prepare/upload. VALID/IDLE/healthy, config1/1, TXblocked,
and persistent automaticON were independently checked over IP and mDNS. The
system endpoint reports the exact 1,063,248-byte image and 247,472-byte margin.
The user subsequently confirmed the requested iPad Safari system-view checks
(metrics, graph refresh, and navigation). This is user-reported UI acceptance,
not an agent-observed Safari trace. The host/build-only section below describes
the earlier implementation stage; it is not the latest deployment status.

The authenticated management page has separate **설정 · OTA** and **시스템** views.
Open 시스템 after login to start monitoring. Returning to the control view,
hiding the browser tab, logging out, or leaving the page stops scheduled reads.
No wiring, partition, configuration-schema, OTA policy, or bus changes are required.

## Read-only contract

`GET /api/v1/system` uses the existing Host gate and SHA-256 Digest authentication.
It returns schemaVersion 1 JSON with `Cache-Control: no-store`. Anonymous reads
receive 401; POST/PUT are unsupported. No credentials, CSRF token, configuration
contents, NVS keys, or firmware upload transaction are included.

The endpoint reuses the existing 2304-byte response buffer and checks snprintf
truncation. There is no new task, response allocation, flash-write path, or
server-side history. During a queued/running GitHub job or active OTA phase the
endpoint returns 409 SYSTEM_BUSY. Reboot may instead cause a connection failure.
The dashboard labels retained values as stale on errors; monitoring never retries
an OTA mutation or changes automatic-update policy.

## Measurements and limits

- `freeHeap`, `minimumFreeHeap`, `largestFreeBlock`: SDK MALLOC_CAP_8BIT statistics
  in bytes. The minimum is the SDK sum of per-heap low-water marks since boot,
  not a continuously sampled trace or a percentage of the nominal 520 KiB SRAM.
- `flashBytes`: detected flash capacity. `runningSlotBytes` / `nextSlotBytes` are
  partition sizes, not available file storage. `runningSlotAddress` identifies
  the running slot.
- `imageBytes`: running image length verified once during startup, cached for
  subsequent reads. Verification failure returns `imageSizeKnown: false`; the
  UI displays unknown rather than interpreting zero as an empty firmware.
- `uploadLimitBytes`: the smaller of the existing upload cap and next OTA slot.
  `firmwareHeadroomBytes` saturates at zero; it is meaningful only when image size
  is known. Neither the firmware cap nor partition layout is changed.
- `nvs` / `diagnosticNvs`: used/free/total **entries**, not filesystem bytes or a
  guarantee that a blob of a particular size can be stored. Failed/uninitialized
  statistics have `available: false` and are shown as unknown, not empty.
- Chip model/revision/core count, CPU frequency (not CPU load), 64-bit uptime in
  seconds, SDK reset-reason code, current IP, and RSSI. RSSI is null without a STA
  connection. No voltage, current, or temperature measurement is inferred.
- `filesystemBytes: 0` describes the current pinned layout, which has no filesystem.
  Unallocated flash is not silently repurposed or presented as writable storage.

Reads are serialized with existing browser operations and scheduled 5 seconds
after completion. There is at most one in-flight operation per page. The chart
retains at most 60 samples in browser memory only; the horizontal axis is sample
order, not a uniform timeline across errors/hidden periods. A lower uptime or
changed build clears the trace. Logout/pagehide clears samples and visible data;
late responses cannot restore a closed session. A same-build reboot that exceeds
the previous uptime before reconnection is not distinguishable from uptime alone.

## Verification (host/build, not deployed)

Candidate build: `usb-bootstrap-0.3.16`, OTA version 316. Existing pinned
PlatformIO atom_lite environment: platform 76b0e9b, Arduino framework
3.20017.241212+sha.dcc1105b, Xtensa 8.4.0+2021r2-patch5. No dependencies changed.

```sh
python3 -m unittest discover -s tests
python3 scripts/validate_m0.py
```

110 tests passed, including 473 C++ transport/runtime assertions and 907 compiled
page JavaScript/HTTP/DOM-fake assertions. Tests cover authentication, wrong Host,
unsupported writes, no NVS/OTA writes, cached image measurement, NVS/image failure,
response bounds, long uptime, RSSI absence, graph bounds/reset, hidden-page pause,
409/429/401, request serialization, and late-response/session teardown.

Exact candidate binary: 1,063,248 bytes, 247,472 bytes (241.67 KiB) below the
1,310,720-byte cap; +11,344 bytes over 315. Static RAM: 79,928 bytes (+16 bytes).
SHA-256: `6a2e5c3c6aa630c831c72e540efe8b2281e3900b27970c57fd10df87999a72e2`.

No release publication, signing/package creation, OTA upload, device policy
change, USB action, or target runtime validation was performed for this feature.
Before release, validate on the exact dm-bridge-8810a1 target: startup/VALID,
Safari layout and navigation, live metrics vs API, prolonged polling, network
loss/reconnect, and OTA health under monitoring. Existing 315 remains the
last-known-good field release; publishing a newer signed release can trigger
its persistent automatic updater and is a separate deployment step.
