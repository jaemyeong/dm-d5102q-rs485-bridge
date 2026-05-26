# Web Admin Auth Gate — Design Spec

- Status: Draft
- Date: 2026-05-27
- Author: Jaemyeong Jin
- Target firmware version: 0.1.9
- Scope: Force HTTP Basic Auth on every web admin and provisioning surface (static HTML, REST API, OTA upload, WebSocket).

## 1. Problem

The web admin currently exposes the entire UI and several APIs without authentication:

- All static pages (`/`, `/dashboard`, `/console`, `/ota`, `/about`) served by `onNotFound` — no auth.
- `GET /api/status`, `GET /api/config`, `GET /api/wifi/scan`, `GET /api/scanner/result` — no auth.
- `POST /update` (OTA firmware upload) — no auth. **Anyone on the LAN can flash arbitrary firmware.**
- `WebSocket /ws/console` — no auth, can receive packets and even queue TX frames.
- Existing `authenticate(req, true/false)` helper is only invoked on a subset of write endpoints, and the `required=false` callers fall through when `basicAuth` is configured off.

Goal: an unauthenticated user must not be able to view or modify the web admin and the AP-mode provisioning screen. Every web surface goes through one authentication gate.

## 2. Decisions (locked in)

| Decision | Choice | Rationale |
|---|---|---|
| What is "provisioning"? | AP-mode initial WiFi setup screen | The same single-page admin UI served while the device is in AP fallback. There is no separate provisioning binary. |
| AP-mode auth | Same web auth as STA | Consistent behavior; AP `apPassword` + web auth = double protection; no special-case provisioning bypass. |
| Auth scheme | HTTP Basic Auth (extended) | Native ESPAsyncWebServer support; minimal new code; browser-native login popup. |
| Coverage | Every endpoint, including static HTML, `/update`, WebSocket | User explicitly chose "all endpoints enforced". |
| Implementation pattern | `AsyncAuthenticationMiddleware` attached globally | Library already exposes middleware API (verified in linked symbols and Context7 docs). One attach point covers every route, including future ones. |
| `basic_auth` toggle | Removed | Authentication is mandatory; no opt-out switch in admin UI or NVS. |

## 3. Architecture

```
incoming HTTP / WS request
        │
        ▼
AsyncWebServer dispatch
        │
        ▼
[global middleware chain]   ← server_.addMiddleware(&authMiddleware_)
        │
        ├─ AsyncAuthenticationMiddleware.run()
        │     ├─ Authorization header present + matches → next()
        │     └─ missing / mismatch → 401 + WWW-Authenticate: Basic realm="DM-D5102Q Bridge"
        │                              (short-circuits chain)
        ▼
route handler (registered via server_.on or onNotFound)
        │
        ▼
business logic (config save, OTA write, factory reset, …)

AsyncWebSocket /ws/console
        │
        └─ ws_.addMiddleware(&authMiddleware_)   ← same instance
            handshake HTTP request goes through the same gate
```

Single `AsyncAuthenticationMiddleware` instance lives as a member of `WebServer`. Its hash is regenerated whenever credentials change (config save path) so the gate honors the new password on the next request without restarting the server.

## 4. Component-level changes

### 4.1 `shared/src/web/web_server.h`

- Add member: `AsyncAuthenticationMiddleware authMiddleware_`
- Remove method: `bool authenticate(AsyncWebServerRequest*, bool required)`
- Add method: `void updateCredentials(const SecurityConfig& sec)` — re-applies username/password and regenerates hash.
- Add private helper (optional): `void configureAuthMiddleware()` — called once from `begin()`.

### 4.2 `shared/src/web/web_server.cpp`

- In `begin()`, after `config_` is bound and before `registerRoutes()`:
  ```cpp
  authMiddleware_.setAuthType(AsyncAuthType::AUTH_BASIC);
  authMiddleware_.setRealm("DM-D5102Q Bridge");
  authMiddleware_.setAuthFailureMessage("Authentication required");
  authMiddleware_.setUsername(config_->security.username.c_str());
  authMiddleware_.setPassword(config_->security.password.c_str());
  authMiddleware_.generateHash();
  server_.addMiddleware(&authMiddleware_);
  ws_.addMiddleware(&authMiddleware_);
  ```
- In `registerRoutes()`, remove every `if (!authenticate(request, …)) return;` call site. The middleware runs first.
- In `handleConfigBody()`, after `store_->save(next)` succeeds and `*config_ = next`, call `updateCredentials(next.security)` to hot-swap credentials.
- Delete the `authenticate()` method body.

### 4.3 `shared/src/storage/config_store.h`

- Remove `bool basicAuth = true;` from `SecurityConfig`. Authentication is unconditional.
- Keep `username` and `password` fields with the existing defaults (`"admin"`).

### 4.4 `shared/src/storage/config_store.cpp`

- Remove `prefs_.getBool("basic_auth", …)` from `load()`.
- Remove `prefs_.putBool("basic_auth", …)` from `save()`.
- `applyDefaults()` keeps the existing precedence: NVS → `BRIDGE_DEFAULT_WEB_USER` / `BRIDGE_DEFAULT_WEB_PASSWORD` (from `secrets.h`) → compiled defaults `"admin"`/`"admin"`.
- Leave any existing `basic_auth` key already written to NVS in place. Not reading it is sufficient — the value becomes inert. No migration code needed.

### 4.5 `shared/src/web/ota_manager.cpp`

- No source changes required. The `server.on("/update", HTTP_POST, …)` registration inherits the global middleware. On 401 the body upload callback is never invoked, so `Update.begin()` is never reached.

### 4.6 `shared/data/index.html` (admin UI)

- In the security/admin section of the settings form, remove the `basic_auth` `<select>` and its label.
- In the JS that serializes the form before `POST /api/config`, drop the `basic_auth` key.
- No additional login UI — browser native Basic Auth popup handles credential entry.
- Verify no other JS path references `basic_auth`. The version pill is updated in §4.9.

### 4.7 `firmware/atom-lite/`, `firmware/atoms3-lite/`

- All source/UI changes are propagated via `tools/sync_shared.sh`. No board-specific edits beyond version bumps (§4.9).

### 4.8 Documentation

- `docs/security.ko.md`, `docs/security.en.md` — replace "Basic Auth Scope Trade-off" with "Mandatory Basic Auth on All Routes". State that `/update`, `/ws/console`, static HTML, and every `/api/*` route require credentials.
- `docs/api/http.ko.md`, `docs/api/http.en.md` — Mark every endpoint row "Auth required: ✓". Drop the `basic_auth` parameter row from `/api/config`. Document `401 Unauthorized` with `WWW-Authenticate: Basic realm="DM-D5102Q Bridge"`.
- `docs/getting-started.ko.md`, `docs/getting-started.en.md` — Note that the first AP-mode connection prompts for `admin/admin` (or the value baked from `secrets.h`) and recommend changing the password before exiting AP mode.
- `README.md`, `README.en.md` — Update security blurb to "All web endpoints require HTTP Basic Auth."
- `CHANGELOG.md`, `CHANGELOG.en.md` — 0.1.9 entry (text in §7).

### 4.9 Version bump (0.1.8 → 0.1.9)

Files to update:
- `firmware/atom-lite/atom-lite.ino` — `FIRMWARE_VERSION` constant.
- `firmware/atoms3-lite/atoms3-lite.ino` — `FIRMWARE_VERSION` constant.
- `shared/data/index.html` — version pill text.
- `firmware/atom-lite/data/index.html`, `firmware/atoms3-lite/data/index.html` — propagated by sync.
- `tests/web_ui_index.test.mjs` — version string assertion.
- `README.md`, `README.en.md` — version badge / heading if applicable.

## 5. Credential lifecycle

### 5.1 First-boot flow (AP mode)

1. Device boots, STA association fails (no WiFi credentials yet) → AP mode (`apSsid` such as `DM-D5102Q-XXXX`).
2. User connects phone/laptop to the AP. `apPassword` from `secrets.h` (if defined) protects the WiFi layer.
3. User opens `http://192.168.4.1/` → browser shows Basic Auth popup (realm "DM-D5102Q Bridge").
4. User enters `admin` / `admin` (or `BRIDGE_DEFAULT_WEB_USER` / `BRIDGE_DEFAULT_WEB_PASSWORD` from `secrets.h`).
5. Admin UI loads. User configures WiFi SSID/password and saves.
6. Device reboots into STA mode. On the new STA IP, the browser prompts for credentials again (different origin). User logs in again, then changes the admin password in the security section.

### 5.2 Hot credential swap

```
user updates password in admin UI
        ↓
POST /api/config (authenticated request)
        ↓
WebServer::handleConfigBody()
        ↓
store_->save(next) succeeds
        ↓
*config_ = next
        ↓
updateCredentials(next.security)
   ├─ authMiddleware_.setUsername(...)
   ├─ authMiddleware_.setPassword(...)
   └─ authMiddleware_.generateHash()
        ↓
response 200 (this response is still under the OLD credentials)
        ↓
next request: browser sends cached old creds → 401 → new popup
```

### 5.3 Credential recovery

- 5-second boot button hold → `factoryResetSettings()` (settings-only NVS namespace wipe). Username/password return to defaults (`admin/admin` or `secrets.h` values). WiFi credentials preserved.
- 8–10-second hold → `factoryResetAll()` (full NVS wipe). Full initial state including AP fallback.
- No serial CLI recovery in this iteration.

### 5.4 Explicit non-goals (YAGNI)

- Password minimum-length validation.
- Brute-force lockout (Basic Auth is stateless; tracking attempts is out of scope).
- Hashed password storage in NVS (physical access threat model is a separate effort).
- Digest auth (Basic chosen by user).
- Mandatory first-login password change.

## 6. Edge cases & verification matrix

| Scenario | Expected behavior |
|---|---|
| `GET /` without credentials | 401 + WWW-Authenticate → browser popup → after entry: 200 + admin UI |
| `GET /api/status` without credentials | 401 (intentional — external monitoring needs auth) |
| `POST /update` (OTA) without credentials | 401, `Update.begin()` never called, no firmware bytes accepted |
| `wscat -c ws://host/ws/console` without credentials | Handshake 401, connection refused |
| Browser admin UI opens WebSocket after page is authenticated | Same-origin cached Basic Auth header automatically attached → handshake succeeds |
| Credential changed while a WebSocket is open | Existing WS stays connected (Basic Auth is verified at handshake only); reconnect requires new creds |
| Credential changed mid-OTA | Subsequent chunks 401, upload fails. Acceptable — users do not change credentials mid-OTA in practice |
| Five wrong attempts | No lockout. Each request is independently evaluated |
| NVS still has `basic_auth` key from a pre-0.1.9 image after OTA | Key is ignored on read; no migration code; no behavioral effect |

## 7. CHANGELOG entry (0.1.9)

```
## 0.1.9 - 2026-05-27
### Changed
- Web admin and provisioning screens now require HTTP Basic Auth on every route (static HTML, /api/*, /update OTA, /ws/console WebSocket).
- Removed `basic_auth` toggle field from the configuration schema and admin UI — authentication is always enforced.

### Security
- /update OTA upload endpoint is now authenticated; previously accessible without credentials.
- /ws/console WebSocket handshake is now authenticated.
- GET endpoints (/api/status, /api/config, /api/wifi/scan, /api/scanner/result) now require credentials.
```

## 8. Tests

### 8.1 New: `tests/web_server_auth.test.mjs`

Static-source assertions over `shared/src/web/web_server.cpp` and `shared/src/web/web_server.h`:

- `addMiddleware(&authMiddleware_)` appears on both `server_` and `ws_` exactly once each.
- `WebServer::begin` source contains all of: `setAuthType(AsyncAuthType::AUTH_BASIC)`, `setRealm(`, `setUsername(`, `setPassword(`, `generateHash()`.
- `WebServer::updateCredentials` source contains: `setUsername(`, `setPassword(`, `generateHash()`.
- No remaining `authenticate(` call sites in `WebServer::registerRoutes` body or in `handle*Body` methods.
- `authenticate(` member function declaration removed from `web_server.h`.
- `updateCredentials(` is called from `handleConfigBody` after a successful `store_->save(`.

Static-source assertions over `shared/src/storage/config_store.{h,cpp}`:

- `bool basicAuth` declaration absent.
- `getBool("basic_auth"` and `putBool("basic_auth"` absent.

### 8.2 Extend: `tests/web_ui_index.test.mjs`

- Assert the admin UI HTML no longer contains a `name="basic_auth"` form control or matching `<select>`.
- Assert version pill matches the new 0.1.9 string.

### 8.3 Compile

- `arduino-cli compile` succeeds for `m5stack:esp32:m5stack_atom` (atom-lite) and `m5stack:esp32:m5stack_atoms3` (atoms3-lite).

### 8.4 Manual checklist (run on flashed firmware)

1. [ ] AP-mode first connect: `http://192.168.4.1/` → Basic Auth popup appears.
2. [ ] Wrong credentials → stay on 401, popup re-appears.
3. [ ] Correct credentials → admin UI loads.
4. [ ] Save WiFi → reboot into STA mode → STA IP requires credentials again.
5. [ ] Each page (Dashboard / Console / OTA / About) without credentials → 401.
6. [ ] `curl http://host/api/status` (no creds) → 401.
7. [ ] `curl -u admin:admin http://host/api/status` → 200 JSON.
8. [ ] `curl -X POST http://host/update -F "data=@firmware.bin"` (no creds) → 401, no flash write.
9. [ ] `wscat -c ws://host/ws/console` (no creds) → handshake 401.
10. [ ] Browser admin UI logged in → WebSocket packets stream normally on `/console`.
11. [ ] Change password to `newpass` → next page navigation prompts for new credentials.
12. [ ] 5-second factory reset button → settings only → credentials revert to default, WiFi retained.
13. [ ] OTA upload (authenticated) → succeeds → device reboots into the new image.
14. [ ] OTA from a pre-0.1.9 image: NVS contains old `basic_auth` key → new firmware boots, credentials work, no warning required.

## 9. Commit plan

| # | Subject | Files |
|---|---|---|
| C1 | `refactor(config): drop basicAuth toggle field` | `shared/src/storage/config_store.{h,cpp}` + sync to two firmware/ trees |
| C2 | `feat(web): enforce auth via AsyncAuthenticationMiddleware on all routes` | `shared/src/web/web_server.{h,cpp}` + sync |
| C3 | `feat(ui): remove basic_auth toggle from admin UI` | `shared/data/index.html` + sync |
| C4 | `test: add web server auth middleware coverage` | `tests/web_server_auth.test.mjs` (new), `tests/web_ui_index.test.mjs` (extend) |
| C5 | `docs: document mandatory web auth across surfaces` | `docs/security.{ko,en}.md`, `docs/api/http.{ko,en}.md`, `docs/getting-started.{ko,en}.md`, `README.md`, `README.en.md`, `CHANGELOG.md`, `CHANGELOG.en.md` |
| C6 | `chore: bump firmware to 0.1.9` | two `.ino` files, `shared/data/index.html` version pill, two firmware `data/index.html` (via sync), `tests/web_ui_index.test.mjs` version assertion |
| C7 | `chore(release): package 0.1.9 OTA artifact` | `releases/dm-d5102q-bridge-0.1.9-<board>-web-auth-gate.bin` (compiled, not committed; produced via `arduino-cli compile`) |

## 10. Out of scope

- TLS / HTTPS. Plaintext Basic Auth is intentional for this iteration; LAN-only deployment is the stated threat model.
- Per-user roles (admin vs read-only). Single-user model retained.
- mDNS / discovery changes.
- Migration of pre-existing NVS `basic_auth` key (left inert).
- Rate limiting or audit logging of failed attempts.
- Replacing the static admin UI with a separate login page.
