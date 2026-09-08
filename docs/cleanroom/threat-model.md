# Threat model baseline

Status: M0 design evidence. Implementation and penetration-test evidence do not
yet exist.

## Assets

- RS485 transmit authority and bus availability
- Wi-Fi and administrator secrets
- configuration integrity and recovery state
- firmware authenticity, active boot slot, and rollback path
- availability of UART receive and bounded queues
- confidentiality of captured operational payloads

## Trust boundaries

1. Physical Atom Lite C008, Tail RS485 T002, field power, and A/B/GND wiring
2. Untrusted clients on the same local network
3. Browser origin, authenticated HTTP API, and WebSocket upgrade
4. Raw TCP read-only service without its own identity layer
5. OTA manifest/image upload and flash boot selection
6. USB serial/bootloader and local maintenance workstation
7. RAM-only capture export crossing into operator-controlled storage

## Adversaries and failures

- a LAN client attempting unauthenticated mutation, resource exhaustion, or
  TCP-to-RS485 injection;
- a malicious web origin attempting CSRF or token reuse;
- a malformed/oversized request whose first body or upload chunk causes a side
  effect before authentication;
- a forged, truncated, wrong-board, or downgraded firmware image;
- a slow TCP/WebSocket client blocking UART receive;
- accidental operator reversal of A/B, wrong termination, unsafe dual power,
  or upload to the non-target Atom S3 Lite;
- reusing A131 bottom-base GPIO22/GPIO19 or power/direction assumptions for
  the selected Tail485, or confusing label-facing and screw-facing terminals;
- a base direction circuit enabling its driver during MCU power-on, reset,
  bootloader entry, UART reconfiguration, or brownout even when firmware never
  writes a UART byte;
- power loss during configuration or OTA;
- extracted flash, local serial access, or RF attacks beyond the default
  trusted-LAN profile.

## Required controls

- TX is compile-time and runtime disabled through M5, then requires authenticated
  request, Origin/CSRF checks, boot-scoped nonce, physical button confirmation,
  expiry, budget, rate limit, idle guard, and queue capacity.
- Every boot, disconnect, OTA, reset, UART reconfiguration, and timeout clears
  arm state; arm state is never persisted.
- API, WebSocket, and OTA share one authentication policy. Body and upload
  callbacks reject before retaining the first byte or changing flash.
- Device-unique credentials replace common defaults; verifiers, not reusable
  plaintext secrets, are stored. Secrets are redacted from all responses/logs.
- All buffers and clients are bounded. Slow clients are disconnected and their
  drops counted without blocking UART receive.
- OTA verifies signed manifest, board ID, size, SHA-256, inactive slot, boot
  health, and rollback before accepting a release.
- Raw TCP is disabled/read-only by default and must not be exposed to WAN.
- Capture is RAM-only unless an authenticated operator explicitly exports it.
- Full reset requires reauthentication, confirmation text, and a short physical
  confirmation window; response flush precedes reset.

## Residual and deferred risks

- Plain HTTP does not protect credentials from an untrusted LAN. Remote access
  requires a VPN or TLS reverse proxy.
- Digest/WebSocket browser interoperability is `UNV` until tested on current
  Chrome and Safari.
- ESP32 secure boot and flash encryption are manufacturing-profile decisions,
  not claims of the development build.
- The T002 direction circuit, GPIO-side logic levels, termination and power
  paths are `UNV` for the selected revision. The manufacturer schematic link
  is only a block diagram; A131 component-level assumptions do not apply. No field connection or M1 upload is allowed before
  power-off continuity and isolated boot/reset scope evidence.
- Field payloads may contain operationally sensitive data; redaction rules need
  bus-owner approval before captures leave the private evidence directory.
