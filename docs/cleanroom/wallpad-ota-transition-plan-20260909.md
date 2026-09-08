# Wallpad-resident development: Wi-Fi recovery and signed OTA

Recorded: 2026-09-09 KST. Updated after the user's correction/implementation approval.
Status: **B1 USB baseline and first signed OTA 0.2.0 → 0.2.1/201 passed; opposite-slot, failed-update recovery and field tests pending**.
See [B1 implementation and operations](signed-ota-bootstrap.md).

## Decision and evidence boundary

The user reports that HTML login and mDNS work, that the configured ATOM Lite
is reachable over Wi-Fi, and requests Tail485 wallpad installation and OTA
development from the Mac. The user subsequently **cancelled button-only reset**:
they had mistaken the initial Wi-Fi connection deadline for a browser-access
deadline. Accept those observations
as user reports; do not repeat the completed onboarding request. No fresh device,
VPN, electrical, RS485 or OTA test was performed for this plan.

The former `usb-bootstrap-0.1.2` had `otaSupported:false`. The user approved
the exact B1 USB image; `usb-bootstrap-0.2.0` reported OTA supported/healthy,
with existing Wi-Fi/key and IP/mDNS access retained. See the
[USB upload receipt](../../.arduino/evidence/upload/signed-ota-bootstrap-upload-20260909.md).
The later `다음` continued one normal OTA to 0.2.1/201: pending health, automatic
confirmation, independent VALID and retained authenticated IP/mDNS access passed.
See the [first OTA receipt](../../.arduino/evidence/upload/signed-ota-bench-201-20260909.md).
No opposite-slot/fault test, watcher or field action is implied by that result.
**Provision an OTA-capable baseline by USB while recovery is easy, prove OTA and
rollback on the bench, and only then consider moving the assembly to the wallpad.**

This plan refines [field OTA development](field-ota-development-plan.md) under
USB-BOOTSTRAP-20260908 and the later HTML/AP and boot-reset/mDNS amendments. The
latest user request retains the shipped Wi-Fi recovery policy, confirms future
button/USB access, reports possible 5 V power and approves remaining implementation.
The earlier planning-only restriction is historical. New image-specific USB review,
live bus gates and unattended automation activation remain separate stages.
Preserve immutable designs/amendments and M0-M9 acceptance.
Physical packets accepted: none. Operational TX remains prohibited.

## 1. Retain existing initial Wi-Fi failure recovery

| Condition | Planned behavior |
| --- | --- |
| Truly unconfigured device | Protected, untimed setup AP and existing HTML onboarding |
| Wi-Fi settings saved successfully | Durable trial, response before restart, then STA |
| New settings fail the initial 60-second Wi-Fi trial | Withdraw the unsuccessful trial and return to the protected setup AP; preserve enrollment key |
| Established settings lose Wi-Fi | Retain settings; retry with existing 5-60 second bounded backoff/jitter |
| Button already held at power-on and continuously qualified for 3 seconds | Existing GPIO39 boot-qualified Wi-Fi-only reset, then protected setup AP |
| Ordinary restart, OTA restart or a late/runtime button press | No Wi-Fi reset; preserve the existing boot-button qualification rules |
| Corrupt committed configuration or failed storage operation | Report/fail closed; do not relabel corruption as empty storage or silently erase it |

Retain both the five-second stable-connection promotion check and the initial
60-second trial withdrawal/AP transition. This timer measures the ATOM's Wi-Fi
connection attempt, not whether a person visits its web page. Once established,
outages keep active settings and use bounded retries without automatic AP fallback.
The existing boot-button Wi-Fi-only reset is also retained. The UI now clarifies
the timer's meaning. Retry scheduling must not repeatedly rewrite flash.

Reset means **Wi-Fi settings only**, as in the approved boot-button feature:
preserve enrollment/admin identity and, once provisioned, OTA verification keys.
No web/API factory-reset path is added. A corrupt storage condition still needs
a functioning physical reset/recovery path; never promise recovery from broken
flash. Firmware rollback is separate from settings reset and must not erase Wi-Fi.

The former 0.1.2, B1 USB 0.2.0 and now-installed OTA 0.2.1 retain this Wi-Fi policy.
The cancelled button-only policy was never implemented or uploaded.

## 2. OTA architecture and security

Selected topology: **Mac deployment controller -> existing LAN or VPN -> ATOM
authenticated HTTP OTA API**. No ESP32 VPN client, cloud dependency or WAN port
forwarding is required. HTTP is not encrypted transport; retain the existing
trusted-management-LAN/VPN threat boundary and do not expose it publicly.

Use the exact registered device identity, not discovery alone. Same-LAN
`dm-bridge-8810a1.local` remains convenient; enroll the numeric IP or approved
private DNS name for routed VPN use, preferably with a DHCP reservation. Check
actual authenticated access over both intended routes before remote reliance.
No router/VPN/DHCP configuration is changed by this plan.

Implement the design's signed OTA protocol rather than assuming ordinary
ArduinoOTA support already exists:

1. Build from pinned inputs; pass host/security tests and the fixed image gate.
2. Produce an immutable manifest with `version`, `boardId`, `imageSize`, `sha256`,
   `minSchema`, `buildId`; sign its precisely specified canonical bytes with
   Ed25519. Bind target and schema checks to the signed manifest.
3. Authenticate prepare before side effects; verify the manifest and issue a
   short-lived, one-use transaction-bound upload token. Specify duplicate-field,
   encoding, length, expiry and replay rejection in the host protocol tests.
4. Authenticate/authorize headers before reading or writing the first upload
   chunk. Preserve same-origin/boot-CSRF rules, bounded streaming and timeouts;
   an upload token does not bypass the management authentication policy.
5. Stream only to the inactive app slot; validate total length and SHA-256 before
   selecting it for boot. Reject wrong board/schema, stale/replayed transactions,
   concurrent uploads, oversized/truncated images and invalid signatures.
6. Restart with TX blocked; run the pending-image health/confirmation protocol
   below. Report build, slot, transaction state and sanitized failure reason.

Private signing keys stay on the approved development host outside the repository,
firmware and logs; firmware contains the verification public key. Device secrets
also stay private. Freeze the supported `dmboot` format for the first OTA baseline;
test retention of active/trial settings and enrollment instead of doing a broad
configuration migration at the same time. Later schema changes must remain
readable by the retained rollback image or require a separate recovery-reviewed
migration. No eFuse changes, secure-boot activation or flash-encryption rollout
are included here; manifest verification is not a hardware secure-boot claim.

## 3. Boot confirmation and real rollback

The local pinned Arduino-ESP32 2.0.17 package provides
`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`. Its `esp32-hal-misc.c` also defaults
`verifyRollbackLater()` to false and `verifyOta()` to true, allowing automatic
confirmation during `initArduino()`, before application health is established.
This is source/config evidence, **not a successful rollback test on this unit**.

The B1 implementation must override the supported deferred-confirmation hook
with the correct linkage, prove that it is linked into the image, and validate
the actual bootloader/partition artifacts used on the device. A source macro or
two app slots alone is insufficient. If those artifacts need changing, resolve
that by a separately reviewed USB bootstrap action while recovery is available,
not by an ordinary field OTA release. No framework upgrade is assumed necessary.

Preserve the design's **30-second pending-image health deadline**. For B1, check
configuration integrity, the main loop/watchdog, Wi-Fi task, memory floors, and
the authenticated management/OTA service; the RS485 driver remains absent and
TX blocked. Later RX firmware adds UART/task liveness without requiring packets
on a legitimately quiet bus and without transmitting test traffic.

Specify an authenticated, transaction/build/boot-bound confirmation handshake:
the controller observes the expected candidate over the management route, then
acknowledges it; firmware marks valid only if its own health checks also pass.
Any confirmation mutation has the same authentication/CSRF/body gates as upload.
Do not use a state-changing GET. No response or wrong build is not success.

An explicit failed health check or missed pending deadline initiates rollback;
watchdog coverage must also reset a hung unconfirmed candidate. A coincident
network outage may prevent candidate confirmation and return the prior image,
but must **not** delete Wi-Fi or open setup AP. The 30-second *OTA confirmation*
deadline is not the removed Wi-Fi onboarding deadline. Ordinary disconnected
boots keep retrying indefinitely.

The [ESP-IDF 4.4.7 OTA documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/ota.html#app-rollback)
describes bootloader rollback configuration and pending-image confirmation.
Validate their behavior on this exact build/device, including reset/power loss;
rollback cannot repair arbitrary bootloader, flash or hardware damage.

Before field installation, establish **two working OTA-capable slots** through
consecutive updates. Leaving only OTA-less 0.1.2 as the fallback would restore
Wi-Fi but lose the next remote-update path. Routine OTA must not rewrite the
bootloader, partition table, enrollment, Wi-Fi settings or verification key.
Automatic rollback to the retained authorized prior image is a recovery event,
not permission to accept arbitrary signed downgrades as normal updates.

## 4. Staged implementation and acceptance

| Stage | Work | Exit evidence |
| --- | --- | --- |
| B1a: retain recovery policy | Cancel the proposed button-only change; clarify Wi-Fi trial versus browser access; retain button/reset/storage behavior | Existing failed-trial/AP and established-outage tests pass; no 60-second browser-access requirement |
| B1b: host-only OTA base/controller | Bounded signed prepare/upload/status/confirmation; deferred boot validation; Mac packaging and deployment command | Positive/negative security, interruption, state, size and compatibility tests; reproducible pinned C008 build; no hardware claims |
| B1c: detached USB bootstrap | Fresh exact target identification, private verified backup, reviewed image-specific USB upgrade preserving settings/key | Verified image/bootloader/partition/NVS evidence and first boot; no Tail485 or field power connected |
| B1d: detached bench OTA | Update A -> B -> C, reachability/config retention, bad-image/bad-boot and power-loss tests in separate controlled trials | At least two consecutive OTA successes; actual rollback to an OTA-capable image; authenticated access afterward; no lost credentials; demonstrated USB recovery |
| F1: Tail485 bench preparation | Complete selected-unit identity, isolated interconnect/power and boot/reset/OTA passivity checks before live bus use | Existing physical evidence gates pass; stable supply and no unintended A/B drive measured on an isolated fixture |
| F2: wallpad power first | Resolve the new possible-5-V report against the actual input/output point; attach only under power-off gates, initially with A/B disconnected | Verified compatible supply, stable power/current/temperature and management access; repeat OTA without bus exposure |
| F3: passive field connection | Connect the approved bus path under isolation/owner gates; start RX-only using measured UART format | Independent passive capture and existing-system health; record any OTA maintenance gap; TX remains prohibited |
| F4: automatic dev channel | Activate the chosen controller/channel after staged tests pass | Eligible release -> one-device update -> expected build/health observed; failed build quarantined and automation halted |

Each hardware row is a sequence of separately approved one-variable experiments,
not permission to combine rewiring, power changes, flashing and fault injection.
Do not perform power-cut/bad-boot testing on the live wallpad bus. B0/B1 development
may precede physical M0 closure, but field/product acceptance retains M0-M9 order.

Build limits remain: 4 MiB flash, two 1.5 MiB app slots, **1,310,720-byte image
ceiling**, no filesystem. The preserved 0.1.2 app is 808,880 bytes, leaving 501,840
bytes shared by OTA/crypto, later RX and UI features. Runtime heap is unverified;
measure steady free heap >=81,920 bytes, loaded free heap >=51,200 bytes and
largest free block >=32,768 bytes with OTA and representative services active.
Do not silently consume reserved flash or raise the ceiling to pass a build.

## 5. Wallpad electrical and service boundaries

The user initially reported nominal 12 V, then said Tail485 power appears to be
5 V. Treat the earlier field-supply assumption as disputed, not as confirmed
12 V and not as proof of a 5 V terminal input. M5Stack documents Tail485 T002
external conversion from 9-24 V to a 5 V ATOM-side supply and public TX26/RX32
mapping. Ask whether 5 V is a measured wallpad-terminal value or an output label.
Do not apply 5 V to the converter input based only on its 5 V output marking.
Actual point, voltage, spare current and compatibility remain unverified.
[Official Tail485 documentation](https://docs.m5stack.com/en/atom/tail485).

Before continuity, mating or wiring changes, isolate **every USB and field/bus
power source** and identify the terminal-wire far ends. Verify actual supply
voltage/polarity, available current, ground/common-mode, connector orientation
and the measured GPIO paths. Do not derive screw-side order from the opposite
label face or assume polarity from wire color. Never feed 12 V directly to the
ATOM's 5 V/GPIO pins; do not combine USB and field power without independently
validated backfeed protection. No live mains work is part of this plan.

Prove Tail485 boot/reset/brownout/OTA passivity on an isolated setup before bus
connection. A firmware TX block and no UART writes do not establish electrical
passivity. Do not assume direction control, add termination/bias, probe actively
or infer Modbus framing without separate evidence. Stop for unexpected drive,
reverse polarity, backfeed, heat/current anomalies or existing-system errors.

After relocation, verify actual Wi-Fi signal and authenticated LAN/VPN access at
the wallpad. Ordinary updates preserve Wi-Fi/management/OTA and stay RX-only.
Single-MCU flashing/reboot can interrupt capture; record maintenance start/end
and actual loss. A fixed-build 72-hour soak needs a new interval after an update;
gap-free capture across OTA would require an independent recorder.

## 6. Automation and recovery operations

Start with a manual signed deployment command, then enable automation for tested
releases promoted to a named `dev` channel. The Mac must be running and reachable
for push deployment; its downtime delays updates, not ordinary device operation.
An always-on alternative controller requires an explicit later choice.

Keep an exact-device registry, artifact hash/build, transaction and health history.
Allow one in-flight update per device and verify current state before each attempt.
For a lost connection/response, reconcile reported transaction/build first. An
offline device waits for an eligible release; it is not an excuse for blind writes.
Bound retry policy, quarantine a failed candidate and stop automatic deployment
until reviewed. Define a maintenance window and retain the two-slot known-good
pair. A later update must never accidentally omit the OTA management service.

The user confirms that button access and USB recovery remain possible after
wallpad installation. This location/access question is closed; no repeat request
is needed. Actual recovery execution remains a bench test, and software rollback
alone is not a guarantee against arbitrary flash/bootloader/hardware damage.

B1b host implementation and B1c exact-image USB installation are complete.
B1d has one normal A-to-B success (0.2.0 → 0.2.1); opposite-slot, bad-image/
bad-boot, power-loss and recovery trials remain pending. No firmware source or
network settings changed for the version-only OTA, and no watcher or field
wiring was activated. Do not repeat completed USB approval or upload.
