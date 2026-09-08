# Field-resident development with automatic OTA

Status: B1 USB baseline and first normal OTA to 0.2.1/201 verified; opposite-slot, failed-update recovery, automation and field deployment pending.
Recorded 2026-09-07 for Atom Lite C008 + Tail RS485 T002.
Authority is the preserved v1.0 design plus TARGET-T002-20260907 and the
approved USB-BOOTSTRAP-20260908 execution amendment.
This plan does not advance M0-M9, authorize a present field upload, or assert
that CI deployment, failed-update rollback or field safety is verified; scoped
OTA implementation and first-transfer evidence are recorded separately below.

2026-09-09 update: the user reports successful HTML login/mDNS and Wi-Fi access,
and subsequently cancels button-only Wi-Fi reset after clarifying that 60 seconds
concerns the ATOM's initial Wi-Fi connection, not browser access. Existing trial
fallback and boot-button reset remain. Button/USB access after installation is
confirmed; the later possible-5-V report needs input/output-point clarification.
The detailed next-stage plan is [wallpad OTA transition](wallpad-ota-transition-plan-20260909.md).
The former 0.1.2 had no OTA receiver. The exact B1 0.2.0 USB installation is now
approved and complete, including first boot, preserved Wi-Fi/key and authenticated
numeric-IP/mDNS status. Initial failed-trial withdrawal after 60 seconds stays.
The later first signed OTA to 0.2.1/201 passed pending-health confirmation and
independent VALID, with retained settings/key and IP/mDNS management. Opposite-
slot OTA, failed-update rollback and field recovery remain pending; no watcher
was activated. See the [first OTA receipt](../../.arduino/evidence/upload/signed-ota-bench-201-20260909.md) and
[B1 USB receipt](../../.arduino/evidence/upload/signed-ota-bootstrap-upload-20260909.md).

## User context and intended sequence

The [user report](../../.arduino/evidence/hardware/user-context-20260907.md)
confirms a current local USB connection and specifies future Tail485 terminal
power/data operation. The desired development loop is:

1. Build and verify an OTA-capable baseline while USB recovery is available.
   Re-identify the exact target before any authorized hardware write.
2. Prove repeated signed OTA, post-boot health and recovery on the bench before
   relying on remote updates. Initial firmware/bootloader/partition provisioning
   is a separate USB action, not a field OTA experiment.
3. Install the verified C008/T002 assembly under the existing wiring, power,
   passivity and bus-owner gates. Keep a practical physical recovery path.
4. Leave it installed and normally online; automatically deploy eligible signed
   development-channel artifacts and verify each resulting device state.

An update may interrupt RX/network service while updating or rebooting. A
single MCU is not a zero-downtime or gap-free capture solution through OTA.
Record update/gap timestamps and actual loss; do not count maintenance gaps
as proof of uninterrupted capture. Keep the separate 72-hour soak free of
firmware changes, or start a new evidence interval for the new build.

## Confirmed first-use onboarding flow

The user explicitly requested AP web onboarding, then a restart into the
configured Wi-Fi network. This maps to baseline sections 8.1, 9.1, 9.3 and
milestones M3-M5; it does not replace their configuration or security gates.

1. On initial USB-provisioned boot, validate stored settings. If genuinely
   unconfigured, enter first-registration provisioning with TX blocked. Corrupt
   committed storage is a separate fail-closed condition, not missing settings.
2. Open the device's protected `DM-BRIDGE-XXXXXX` AP without an elapsed-time limit,
   as explicitly approved in WEB-LOGIN-AP-20260909.
   Retain the per-device random enrollment key (at least 12 characters), the
   one-time USB installation handoff and the currently implemented power-on,
   continuously held 3-second Wi-Fi-only reset. Do not substitute an open AP or
   shared password. The old ten-minute limit is superseded, not reintroduced.
3. After joining the AP, the installer opens the local onboarding web page,
   selects/enters the field Wi-Fi SSID and secret, and submits valid settings.
   Use actual device scan/results and show errors; do not assume a captive
   browser window opens automatically. Secrets are never returned by APIs/logs.
4. Validate and atomically store/read-back the configuration using the existing
   two-slot NVS contract. Return the saved revision and explicit
   `restartRequired`/`reconnectExpected` result before the requested restart;
   a failed save must not be reported as successful or trigger success reboot.
5. Reboot, close the onboarding AP after successful configuration storage,
   enter STA and expose the authenticated web/OTA API on the approved LAN.
   The Mac can then reach the same management endpoint locally or over VPN.
6. On subsequent normal boot or OTA, reuse valid stored network/admin settings;
   do not repeat onboarding. A transient STA failure is not missing settings:
   retry with the baseline bounded backoff, keep secrets/settings and show the
   actual disconnected state. Unsuccessful initial trials still withdraw after
   60 seconds and return to protected setup AP; this is not a browser-access timer.
   The user cancelled the proposed button-only restriction before implementation.

Before field enrollment, prove missing/valid/corrupt-config boots, bad Wi-Fi
credential trial withdrawal, physical reset/AP re-entry, failed/interrupted save,
response-before-reboot, both LAN/VPN API access and OTA preserving settings.
The user reports HTML login/mDNS and Wi-Fi access; remaining recovery, VPN and
The first normal OTA is now completed bench evidence as linked above; failure,
repeated-slot and field OTA scenarios remain planned tests, not passing evidence.

## Automation policy to implement

- Build from immutable inputs and run tests, image-size and security gates
  before an artifact becomes eligible for the selected development channel.
  A source save, failed build, or arbitrary branch is not a deployment event.
- Generate the existing version/boardId/imageSize/sha256/minSchema/buildId
  manifest and Ed25519 signature. Keep the signing key and device credentials
  outside the repository, device image and logs. Do not create accounts,
  secrets, remote services, CI jobs or deployment schedules in this iteration.
- The deployment controller uses the authenticated prepare/upload/status API;
  retain first-chunk authentication, Origin/CSRF controls, bounded sizes and
  short-lived upload tokens. No unauthenticated ArduinoOTA shortcut is implied.
- Default to one exact device, one image and one deployment in flight. First
  field enrollment/channel activation needs the agreed safety and network
  prerequisites; thereafter eligible artifacts may deploy automatically.
- Check reported target, current build, schema compatibility and updater state
  before upload. Preserve a known-good image and compatible settings. Ordinary
  development releases must preserve Wi-Fi, credentials, schema, bootloader,
  partition layout and the OTA recovery service. Changes to those are separate
  high-risk recovery-reviewed releases, not routine automatic updates.
- Stream to the inactive slot, validate signature/hash/size before selecting
  boot, and retain the design's 30-second health deadline. Health must cover
  UART task liveness, configuration, Wi-Fi task and the OTA management path;
  Wi-Fi association alone is not enough. Do not send RS485 test frames as a
  health probe or require traffic when the bus is legitimately idle.
- Confirm success only after the controller observes the expected buildId,
  healthy state and continuing management reachability. A lost upload response
  is an unknown outcome: reconcile status before retrying, never flash blindly.
- On failed confirmation, use the verified rollback/recovery path, quarantine
  that build for this device and halt further automatic deployment until the
  cause is resolved. Bound retries; do not alternate endlessly between images.
- Automatic firmware updates never authorize operational RS485 TX. Preserve
  RX-only/default TX-blocked behavior; OTA/reset disarms TX and never restores
  a persisted arm state. M6 and a separate field approval still govern TX.

## User-confirmed management network

The current design assumes a trusted LAN, forbids WAN port forwarding, and
places remote access behind a VPN or TLS reverse proxy (sections 1.2, 12.2,
12.3). A controller on that approved management network can automate the
existing device API without adding an on-device cloud client.

The user confirms both same-LAN and VPN access. Select a controller on the Mac
or another explicitly approved reachable host; the ESP32 need not run a VPN
client or an internet-facing/cloud pull updater. Reuse the device's management
endpoint across those routes. Resolve a stable management address at enrollment
instead of depending only on discovery. Actual routing, API authentication and
post-OTA access still need testing after provisioning.

Controller placement, eligible development channel, signing-key custody and
maintenance timing remain deployment configuration choices, not reasons to
repeat the answered topology question. No VPN, firewall, account, CI or remote
service changes have been made or authorized by this plan.

## Evidence required before relying on field automation

M8 must establish authentication rejection before writes, signed-image negative
tests, interrupted transfer/power-loss recovery, repeated OTA, retained OTA
service, network/config preservation, bad-boot rollback and safe TX behavior.
M9 must record field power/network recovery, actual update interruption, health
observability, physical USB recovery arrangements and a fixed-build soak.
No failure injection on a live field bus is authorized by this plan.

The [Espressif ESP-IDF 4.4.7 OTA documentation](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/ota.html#app-rollback)
requires bootloader rollback configuration and first-boot confirmation for its
rollback state machine. Two application slots alone do not prove recovery.
The pinned package enables rollback but Arduino's default early confirmation
must be deferred until health checks pass. Exact flashed artifacts and real
failure recovery still need bench verification; do not infer success from
framework support or the historical heartbeat upload.

USB-BOOTSTRAP-20260908 allows standalone B0 AP/STA host development and later
B1 signed-OTA development before physical M0 closure, without promoting product
acceptance. B0 0.1.2 has verified USB upload/first boot and subsequent user-reported
login/mDNS and Wi-Fi access. B1 signed OTA now has host code/build/tests, verified
USB baseline and one normal 0.2.0 → 0.2.1 OTA/confirmation success. Actual failed-
update recovery is not yet target-tested. See [signed OTA bootstrap](signed-ota-bootstrap.md),
[USB bootstrap](usb-bootstrap.md) and the dated transition plan above.
Existing M0 criteria, 207 baseline criterion identities, toolchain inputs, source locks,
private evidence, the rollback backup and accepted-packet count stay unchanged.
