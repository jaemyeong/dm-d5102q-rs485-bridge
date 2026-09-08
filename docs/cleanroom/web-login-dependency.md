# Offline browser SHA-256 dependency

Selection checked 2026-09-09 KST. js-sha256 is a **third-party community**
library authored by Chen, Yi-Cyuan (emn178), not vendor-authored firmware or
an official M5Stack/Arduino/Espressif dependency. The selected version is
0.11.1; this is a deliberate pin, not a claim that it is the latest release.

- Upstream: https://github.com/emn178/js-sha256
- Tag: `v0.11.1`, annotated tag object `06c1abf9b1a7a5291bb089dbf3f8b272ba4e022b`,
  peeled source commit `7912494a5ae277660131557401b91f11f47c96d2`.
- Artifact: `build/sha256.min.js` from that tag, version header 0.11.1
- License: MIT; full license from the same tag is included in the served page.
- Local wrapper: `firmware/usb_bootstrap/web_sha256.inc`, SHA-256
  `4de27631e00b4708d207819cc9630e18313ceac8348fb36ed5f374903e545770`.
- The raw C++ literal adds source metadata and license only; the upstream
  minified JavaScript is unchanged. `validate_usb_bootstrap.py` pins all bytes.

The dependency has no browser runtime dependencies, CDN calls or browser
storage. Its browser SHA-256 implementation supports UTF-8. Upstream also
ships SHA-224/HMAC and a Node crypto fast path; these are not used by the UI.
Tests run the actual vendored code in a VM without process/require/module so
the browser implementation, not Node's fast path, is exercised. Independent
Node crypto and the C++ mbedTLS/CommonCrypto adapter verify digest results.

The choice avoids hand-implementing SHA-256 and avoids requiring
`crypto.subtle.digest`, which is unavailable on an ordinary HTTP AP origin.
Client nonce generation still requires `crypto.getRandomValues`, supported in
an insecure context per the [MDN API reference](https://developer.mozilla.org/en-US/docs/Web/API/Crypto/getRandomValues).
There is no Math.random fallback or MD5 downgrade. Maintenance/version changes
require explicit source/license review, hash update, vectors, client/server
interoperability tests and a new firmware image budget check.

API fetches explicitly set `credentials: 'omit'` and supply the computed
Authorization header themselves. The [WHATWG Fetch authentication algorithm](https://fetch.spec.whatwg.org/#http-network-or-cache-fetch)
gates its 401 browser-prompt/retry branch on included credentials. The public
challenge route also returns 200 with JSON, not a native authentication challenge.
This is a standards-backed design choice, not an actual iPad browser test.

HTTP serves both the page and API and is **not encrypted** by this mechanism.
An attacker able to replace the page can steal input. The protected AP/trusted
LAN/VPN restriction remains essential. No audit, vulnerability-free status,
forensic JavaScript memory erasure or actual Safari test is asserted.
