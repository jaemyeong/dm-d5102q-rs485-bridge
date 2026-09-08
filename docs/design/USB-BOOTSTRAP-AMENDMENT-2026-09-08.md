# USB-BOOTSTRAP-20260908

Approval: the user replied `승인` on 2026-09-08 to the proposed USB-first
development order. This is an additive execution amendment to design v1.0
(SHA-256 `d2ed6064f8bb4babc75a7877060684294728eaa9cefb351817ca33f9e2584114`)
and TARGET-T002-20260907, not a replacement of either source.

## B0: standalone USB onboarding

Host implementation, tests and exact C008 builds may precede completion of M0
physical evidence. The user reports Tail485 detached and Atom Lite connected
to the development Mac by USB. B0 application code initializes no RS485 UART,
GPIO, button, LED, I2C or SPI. USB Serial0 is used for setup diagnostics only.
The sole `atom_lite` PlatformIO environment now builds `firmware/usb_bootstrap`;
the old no-I/O smoke fixture remains preserved. Toolchain, warning/LTO flags,
partitions, image ceiling and immutable physical-evidence gates are unchanged.

B0 implements protected, ten-minute AP onboarding, authenticated web Wi-Fi
setup, checked persistent storage, an explicit save/reboot response and STA
connection. It has no RS485, raw TCP bridge, scanner, OTA endpoint or automatic
deployment. Read-only HTTP management is available by numeric device IPv4 on
the LAN or through an existing routed VPN. DNS discovery and captive-portal
redirects are deferred. Never expose this HTTP interface to the public WAN.

The installation key is generated per device and handed off once over USB
when first created; retain it privately before onboarding. It protects both
the WPA2 AP and HTTP Digest SHA-256 (`installer` username). Network mutations
require successful header authentication and same-origin boot CSRF validation
before any body bytes are read by the application. Buffers and deadlines are
fixed. HTTP Digest is not transport encryption: WPA2 protects the onboarding
radio link; trusted LAN or VPN is required for subsequent HTTP access.

### B0-specific configuration format and recovery

This deliberately isolated bootstrap profile uses the `dmboot` NVS namespace,
not the final product configuration schema. Two fixed 112-byte records contain
magic, schema, revision, lengths, SSID/password and CRC32. Explicit CRC-protected
`trial` and `active` references commit a slot and revision; unreferenced records
are never activated merely because they have a higher generation. Write,
read-back and commit failures fail closed. This refines the baseline's suggested
highest-valid-generation selection to avoid activating an uncommitted write.

Only initial configuration is writable in B0. Saving creates a trial, then
reboots. A continuously connected STA with IPv4 for five seconds promotes that
trial to active. A trial that cannot connect within sixty seconds is withdrawn
before opening a fresh timed provisioning AP. Known-good active settings are
retained during outages; reconnect uses bounded exponential backoff and jitter,
not automatic AP fallback. Invalid/corrupt committed storage stops networking
and requires a separate, authorized USB recovery; it is never silently erased.
Open networks and WEP are not supported. Passwords are 8-63 printable ASCII
characters or a 64-hex-digit PSK. SSID length is 1-32 valid UTF-8 bytes.

B0 has no established-device Wi-Fi reset or password rotation UI. Later
firmware must explicitly migrate `dmboot` (including credentials) before field
deployment; an ordinary update must not discard working network settings.
NVS/flash encryption and bootloader rollback are not established by this phase.

## Subsequent order and unchanged acceptance gates

After B0 host/build evidence: obtain image-specific USB upload approval,
re-identify exact VID/PID/serial and chip/flash size, and preserve private
rollback/credential handoff. Then verify actual AP/browser/save/reboot/STA
behavior on the detached C008. No device access or upload is implied by host
build success. Existing photo receipts and USB observations are not rewritten.

B1 will implement and verify signed OTA, reachability, health confirmation and
real rollback before unattended development deployment. Tail485 identity,
de-energized wiring, power-path/12 V, boot passivity and field permissions must
pass before module/field work. Terminal-wire far-end observations are deferred
to that applicable physical step, not required for host-only B0 development.

Product acceptance still completes M0-M9 in order. B0/B1 development does not
promote M3/M4/M5/M8 or waive any M0 stop condition, TX gate or M9 release gate.
The 207-criterion catalogue, original state history, raw evidence and ledger
prefix remain preserved. Report host, build, upload, hardware, system and
deployment proof separately. Initial host-only completion is not field-ready.
