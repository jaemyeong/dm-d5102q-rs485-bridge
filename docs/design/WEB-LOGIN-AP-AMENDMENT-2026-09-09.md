# WEB-LOGIN-AP-20260909

Approval: the user requested `html 로 간단하게 로그인화면 만들어서 진행하자`
and then explicitly added `10분제한도 제거해` on 2026-09-09 KST.
This is an additive B0 amendment to design v1.0 and
USB-BOOTSTRAP-20260908. Their original bytes and historical evidence remain
unchanged. Only the following browser and provisioning policies are superseded.

## Browser login without a native authentication dialog

GET `/` serves a public, static Korean HTML login shell containing no credentials,
configuration, CSRF token or current device state. GET `/api/v1/auth` returns
only public Digest challenge metadata (realm, nonce, algorithm and qop), with
the existing request/authentication rate limits. These two bodyless GET routes
do not require authentication. No other route gains anonymous access.

The form uses the existing `installer` identity and per-device installation key.
An offline, pinned MIT-licensed community JavaScript SHA-256 implementation
constructs standard HTTP Digest SHA-256 Authorization headers. This avoids
Safari's native Digest SHA-256 dialog dependency and does not require Web Crypto
digest on the non-secure HTTP AP origin. Cryptographic random cnonce generation
is mandatory; there is no Math.random fallback. No CDN or external assets load.

Only the derived HA1 value and random client nonce live in page memory while
logged in. The key input is cleared after derivation, and logout/pagehide clears
the page's authentication state. No password, HA1 or token is stored in cookies,
localStorage, sessionStorage or URLs. JavaScript cannot promise forensic memory
zeroization. Password-manager behavior is browser-controlled.

GET status remains authenticated. PUT configuration still requires Digest and
same-origin boot CSRF validation before the application reads any body byte.
The existing five-minute nonce expiry, replay counters, HTTP size/deadline and
rate limits remain. The client obtains fresh challenge metadata before each
signed operation; it never automatically retries a configuration write. A lost
save response is ambiguous and must not be reported as success or auto-replayed.
There is no MD5/Basic downgrade or plaintext installation-key login endpoint.

HTTP still provides no transport encryption or protection from a malicious
trusted-network intermediary replacing the served page. Use only the protected
setup AP, trusted LAN or existing VPN; never public WAN. This is not a new TLS,
session-cookie, credential-rotation or secure-boot implementation.

## Provisioning AP has no elapsed-time expiry

The ten-minute limit in original design section 8.1 and the B0 amendment is
removed for this B0 revision. While configuration is empty, the password-protected
AP stays available until an explicit state transition, reboot/power loss or fault;
elapsed time alone never closes it. This also applies after a failed initial
sixty-second STA trial is withdrawn. Status explicitly reports
`apTimeoutEnabled: false` and `apRemainingMs: null`, not zero seconds remaining.

A successful save still stages configuration, finishes its response and reboots
into STA-only trial mode, closing the AP. Five seconds of stable IPv4 connectivity
promotes the trial. The sixty-second trial timeout and reconnect backoff are
unchanged. A configured device does not open an AP during a Wi-Fi outage.
Removing the AP deadline does not remove request, nonce or STA trial timeouts.
Longer provisioning exposure is an explicit user-selected tradeoff; AP password
protection, one-station limit and all API authentication guards remain.

## Scope and evidence

Build ID advances to `usb-bootstrap-0.1.1`. NVS format, installation key,
toolchain, partition layout, 1,310,720-byte image ceiling, no-GPIO/no-bus boundary
and all physical/TX gates remain unchanged. Browser assets stay inside the
application image; reserved unallocated flash is not a new filesystem allocation.

This authorizes host implementation, tests and an exact C008 candidate build.
It does not authorize another USB upload, NVS erase, module/power action, Mac
network change or unattended OTA. A new image requires its own USB approval and
exact-device identification, private backup and credential-preserving workflow.
Host DOM/HTTP tests are not an iPad Safari or physical AP/STA observation. M0-M9
and field deployment acceptance are not advanced by this amendment.
