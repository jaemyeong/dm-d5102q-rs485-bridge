# Web Admin Auth Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Force HTTP Basic Auth on every web admin and provisioning surface (static HTML, REST API, OTA upload, WebSocket) by attaching one `AsyncAuthenticationMiddleware` globally to the ESPAsyncWebServer instance. Ship as firmware 0.1.9.

**Architecture:** Add a single `AsyncAuthenticationMiddleware` member to `dm::WebServer`, initialize it from `DeviceConfig::SecurityConfig` in `begin()`, attach it to both the HTTP server and the AsyncWebSocket. Delete the per-route `authenticate()` helper. Hot-swap credentials when `/api/config` saves a new password. Remove the `basic_auth` toggle field from `SecurityConfig` and the admin UI; authentication is unconditional.

**Tech Stack:** Arduino C++17, ESPAsyncWebServer (ESP32Async fork — `AsyncAuthenticationMiddleware` confirmed via linked symbols), ESP32 Preferences NVS, vanilla JS admin UI, Node ESM tests (`node --test`).

**Spec:** `docs/superpowers/specs/2026-05-27-web-admin-auth-gate-design.md`

---

## File Structure

| File | Action | Responsibility |
|---|---|---|
| `shared/src/storage/config_store.h` | Modify | Remove `bool basicAuth` from `SecurityConfig`. |
| `shared/src/storage/config_store.cpp` | Modify | Remove `basic_auth` NVS read/write. |
| `shared/src/web/web_server.h` | Modify | Replace `authenticate()` declaration with middleware member + `updateCredentials()`. |
| `shared/src/web/web_server.cpp` | Modify | Configure middleware in `begin()`, drop `authenticate()` definition and all call sites, hot-swap on config save. |
| `shared/data/index.html` | Modify | Remove `basic_auth` `<select>` and JS references. Bump version pill and JS default. |
| `shared/src/app/firmware_app.cpp` | Modify | Bump boot log version string. |
| `shared/src/status/device_status.cpp` | Modify | Bump JSON `device.version` value. |
| `tests/web_server_auth.test.mjs` | Create | Static-source assertions enforcing the new auth model. |
| `tests/web_ui_index.test.mjs` | Modify | Add `basic_auth` absence assertion; bump version string. |
| `docs/security.md`, `docs/security.en.md` | Modify | Document mandatory auth across all surfaces. |
| `docs/api/http.md`, `docs/api/http.en.md` | Modify | Mark every endpoint as "Auth required: ✓"; drop `basic_auth` parameter. |
| `docs/getting-started.md`, `docs/getting-started.en.md` | Modify | Document first-login flow with default credentials. |
| `README.md`, `README.en.md` | Modify | Update security blurb and version line. |
| `CHANGELOG.md`, `CHANGELOG.en.md` | Modify | Add 0.1.9 entry. |
| `tools/sync_shared.sh` | Execute | Propagate `shared/` to `firmware/atom-lite/` and `firmware/atoms3-lite/`. Run at the end of each task that touches `shared/`. |

After each shared-tree edit, the engineer **must** run `tools/sync_shared.sh` before committing so the two board-specific trees stay in lock-step. The script also re-runs `tools/embed_web_ui.mjs` to regenerate the gzipped `admin_ui.h` for both boards.

---

## Task 1: Remove `basicAuth` toggle field from `SecurityConfig`

**Files:**
- Modify: `shared/src/storage/config_store.h:43`
- Modify: `shared/src/storage/config_store.cpp:43,81` (and any nearby `basic_auth` references)
- Create: `tests/web_server_auth.test.mjs`

- [ ] **Step 1: Write the failing static-source test**

Create `tests/web_server_auth.test.mjs`:

```javascript
import assert from "node:assert/strict";
import { test } from "node:test";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(dirname(fileURLToPath(import.meta.url)));

const configStoreHeader = readFileSync(join(root, "shared/src/storage/config_store.h"), "utf8");
const configStoreSource = readFileSync(join(root, "shared/src/storage/config_store.cpp"), "utf8");

test("SecurityConfig no longer exposes basicAuth toggle", () => {
  assert.doesNotMatch(configStoreHeader, /\bbool\s+basicAuth\b/);
});

test("ConfigStore does not read or write basic_auth NVS key", () => {
  assert.doesNotMatch(configStoreSource, /"basic_auth"/);
});
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `node --test tests/web_server_auth.test.mjs`

Expected: Both tests FAIL (matches found in current sources for `bool basicAuth` and `"basic_auth"`).

- [ ] **Step 3: Remove the field declaration**

In `shared/src/storage/config_store.h`, find the `SecurityConfig` struct (around line 41-46) and delete the `basicAuth` field. Before:

```cpp
struct SecurityConfig {
  bool basicAuth = true;
  String username = "admin";
  String password = "admin";
};
```

After:

```cpp
struct SecurityConfig {
  String username = "admin";
  String password = "admin";
};
```

- [ ] **Step 4: Remove the NVS read of `basic_auth`**

In `shared/src/storage/config_store.cpp`, locate the `load()` function. Delete the line:

```cpp
config.security.basicAuth = prefs_.getBool("basic_auth", config.security.basicAuth);
```

Leave the surrounding `config.security.username = prefs_.getString("auth_user", ...)` and `config.security.password = prefs_.getString("auth_pass", ...)` lines untouched.

- [ ] **Step 5: Remove the NVS write of `basic_auth`**

In the same file, locate the `save()` function. Delete the line:

```cpp
prefs_.putBool("basic_auth", config.security.basicAuth);
```

Leave the surrounding `prefs_.putString("auth_user", ...)` and `prefs_.putString("auth_pass", ...)` lines untouched.

- [ ] **Step 6: Verify no other references**

Run: `grep -rn "basicAuth\|basic_auth" shared/src/`

Expected: No matches. (Any remaining match is a leftover that must be removed before moving on.)

- [ ] **Step 7: Run tests to verify they pass**

Run: `node --test tests/web_server_auth.test.mjs`

Expected: Both tests PASS.

- [ ] **Step 8: Sync shared tree**

Run: `tools/sync_shared.sh`

Expected: Script completes silently. `firmware/atom-lite/src/storage/config_store.{h,cpp}` and `firmware/atoms3-lite/src/storage/config_store.{h,cpp}` now mirror the shared tree.

- [ ] **Step 9: Commit**

```bash
git add shared/src/storage/config_store.h shared/src/storage/config_store.cpp \
        firmware/atom-lite/src/storage/config_store.h firmware/atom-lite/src/storage/config_store.cpp \
        firmware/atoms3-lite/src/storage/config_store.h firmware/atoms3-lite/src/storage/config_store.cpp \
        tests/web_server_auth.test.mjs
git -c commit.gpgsign=false commit -m "refactor(config): drop basicAuth toggle field

Authentication is unconditional in 0.1.9, so the SecurityConfig
toggle and its NVS key serve no purpose. Pre-existing 'basic_auth'
NVS values become inert and are ignored on read."
```

---

## Task 2: Attach `AsyncAuthenticationMiddleware` globally on `WebServer`

**Files:**
- Modify: `shared/src/web/web_server.h:32` (remove `authenticate` declaration, add middleware + `updateCredentials`)
- Modify: `shared/src/web/web_server.cpp` (configure in `begin()`, delete `authenticate()` body, remove all call sites, call `updateCredentials` in `handleConfigBody`)

- [ ] **Step 1: Extend `tests/web_server_auth.test.mjs` with the failing assertions**

Append to `tests/web_server_auth.test.mjs`:

```javascript
const webServerHeader = readFileSync(join(root, "shared/src/web/web_server.h"), "utf8");
const webServerSource = readFileSync(join(root, "shared/src/web/web_server.cpp"), "utf8");

test("WebServer no longer declares the authenticate() helper", () => {
  assert.doesNotMatch(webServerHeader, /\bbool\s+authenticate\s*\(/);
});

test("WebServer holds a single AsyncAuthenticationMiddleware member", () => {
  assert.match(webServerHeader, /AsyncAuthenticationMiddleware\s+authMiddleware_/);
});

test("WebServer declares updateCredentials hot-swap entry point", () => {
  assert.match(webServerHeader, /void\s+updateCredentials\s*\(\s*const\s+SecurityConfig&/);
});

test("WebServer::begin configures the middleware before registering routes", () => {
  for (const fragment of [
    "authMiddleware_.setAuthType(AsyncAuthType::AUTH_BASIC)",
    "authMiddleware_.setRealm(",
    "authMiddleware_.setUsername(",
    "authMiddleware_.setPassword(",
    "authMiddleware_.generateHash()",
    "server_.addMiddleware(&authMiddleware_)",
    "ws_.addMiddleware(&authMiddleware_)",
  ]) {
    assert.ok(
      webServerSource.includes(fragment),
      `expected WebServer source to contain: ${fragment}`,
    );
  }
});

test("WebServer::updateCredentials regenerates the auth hash", () => {
  const fnMatch = webServerSource.match(/void\s+WebServer::updateCredentials[\s\S]*?\n\}/);
  assert.ok(fnMatch, "updateCredentials definition missing");
  const body = fnMatch[0];
  assert.match(body, /setUsername\(/);
  assert.match(body, /setPassword\(/);
  assert.match(body, /generateHash\(\)/);
});

test("registerRoutes contains no remaining authenticate() call sites", () => {
  const routesMatch = webServerSource.match(/void\s+WebServer::registerRoutes[\s\S]*?\n\}\s*\n/);
  assert.ok(routesMatch, "registerRoutes definition missing");
  assert.doesNotMatch(routesMatch[0], /\bauthenticate\s*\(/);
});

test("handleConfigBody calls updateCredentials after save", () => {
  const fnMatch = webServerSource.match(/void\s+WebServer::handleConfigBody[\s\S]*?\n\}/);
  assert.ok(fnMatch, "handleConfigBody definition missing");
  assert.match(fnMatch[0], /updateCredentials\s*\(/);
});
```

- [ ] **Step 2: Run the new assertions to confirm they fail**

Run: `node --test tests/web_server_auth.test.mjs`

Expected: The seven new tests FAIL (current source still declares `authenticate(`, has no middleware member, no `updateCredentials`, etc.). The two Task 1 tests still PASS.

- [ ] **Step 3: Update the header**

In `shared/src/web/web_server.h`, replace the `bool authenticate(...)` declaration with the middleware member and the new hot-swap entry point.

Before (around lines 30-44):

```cpp
 private:
  using BodyHandler = void (*)(WebServer*, AsyncWebServerRequest*, const String&);

  bool authenticate(AsyncWebServerRequest* request, bool required);
  void sendJson(AsyncWebServerRequest* request, JsonDocument& doc, int status = 200);
  void sendError(AsyncWebServerRequest* request, int status, const char* code);
  void registerRoutes();
  void handleConfigBody(AsyncWebServerRequest* request, const String& body);
```

After:

```cpp
 public:
  void updateCredentials(const SecurityConfig& security);

 private:
  using BodyHandler = void (*)(WebServer*, AsyncWebServerRequest*, const String&);

  void sendJson(AsyncWebServerRequest* request, JsonDocument& doc, int status = 200);
  void sendError(AsyncWebServerRequest* request, int status, const char* code);
  void registerRoutes();
  void handleConfigBody(AsyncWebServerRequest* request, const String& body);
```

Add the middleware member alongside the other members (after `OtaManager ota_;`):

```cpp
  OtaManager ota_;
  AsyncAuthenticationMiddleware authMiddleware_;
```

- [ ] **Step 4: Configure the middleware in `begin()`**

In `shared/src/web/web_server.cpp`, replace the body of `WebServer::begin` (lines 26-46). Before:

```cpp
void WebServer::begin(DeviceConfig& config, ConfigStore& store, DeviceStatus& status, BaudScanner& scanner) {
  config_ = &config;
  store_ = &store;
  status_ = &status;
  scanner_ = &scanner;

  ws_.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    handleWsEvent(server, client, type, arg, data, len);
  });
  server_.addHandler(&ws_);
  registerRoutes();
  ota_.begin(server_, status);
  server_.onNotFound([](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_GET && !request->url().startsWith("/api/")) {
      sendAdminUi(request);
      return;
    }
    request->send(404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
  });
  server_.begin();
}
```

After:

```cpp
void WebServer::begin(DeviceConfig& config, ConfigStore& store, DeviceStatus& status, BaudScanner& scanner) {
  config_ = &config;
  store_ = &store;
  status_ = &status;
  scanner_ = &scanner;

  authMiddleware_.setAuthType(AsyncAuthType::AUTH_BASIC);
  authMiddleware_.setRealm("DM-D5102Q Bridge");
  authMiddleware_.setAuthFailureMessage("Authentication required");
  authMiddleware_.setUsername(config_->security.username.c_str());
  authMiddleware_.setPassword(config_->security.password.c_str());
  authMiddleware_.generateHash();
  server_.addMiddleware(&authMiddleware_);
  ws_.addMiddleware(&authMiddleware_);

  ws_.onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
    handleWsEvent(server, client, type, arg, data, len);
  });
  server_.addHandler(&ws_);
  registerRoutes();
  ota_.begin(server_, status);
  server_.onNotFound([](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_GET && !request->url().startsWith("/api/")) {
      sendAdminUi(request);
      return;
    }
    request->send(404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
  });
  server_.begin();
}
```

- [ ] **Step 5: Delete the `authenticate()` definition**

In `shared/src/web/web_server.cpp`, delete the entire `WebServer::authenticate` function (currently lines 83-88):

```cpp
bool WebServer::authenticate(AsyncWebServerRequest* request, bool required) {
  if (!required || !config_ || !config_->security.basicAuth) return true;
  if (request->authenticate(config_->security.username.c_str(), config_->security.password.c_str())) return true;
  request->requestAuthentication();
  return false;
}
```

- [ ] **Step 6: Remove every `authenticate(...)` call site**

In `shared/src/web/web_server.cpp`, remove each of these lines (the middleware now performs the check before any handler runs):

- In the `/api/config` GET handler: `if (!authenticate(request, false)) return;`
- In the `/api/scanner/stop` POST handler: `if (!authenticate(request, true)) return;`
- In the `/api/factory-reset/settings` handler: `if (!authenticate(request, true)) return;`
- In the `/api/factory-reset/all` handler: `if (!authenticate(request, true)) return;`
- In `handleConfigBody`: `if (!authenticate(request, true)) return;` (top of the function)
- In `handleTxBody`: `if (!authenticate(request, true)) return;` (top of the function)
- In `handleScannerStartBody`: `if (!authenticate(request, true)) return;` (top of the function)
- In `handleWifiScan`: `if (!authenticate(request, false)) return;` (top of the function)

After this step, no `authenticate(` substring should remain anywhere in `shared/src/web/web_server.cpp` except inside the soon-to-be-added `updateCredentials` body (which calls library API methods, not the deleted helper).

- [ ] **Step 7: Add the `updateCredentials` definition**

In `shared/src/web/web_server.cpp`, place this new function immediately after `WebServer::setFactoryResetHandler` and before `WebServer::poll`:

```cpp
void WebServer::updateCredentials(const SecurityConfig& security) {
  authMiddleware_.setUsername(security.username.c_str());
  authMiddleware_.setPassword(security.password.c_str());
  authMiddleware_.generateHash();
}
```

- [ ] **Step 8: Hot-swap credentials when config saves**

In `shared/src/web/web_server.cpp`, modify `handleConfigBody`. Locate the success branch (currently around lines 213-217):

```cpp
  bool ok = store_->save(next);
  if (ok) {
    *config_ = next;
    if (saveHandler_) saveHandler_(next, saveCtx_);
  }
```

Change it to also refresh middleware credentials when the save succeeds:

```cpp
  bool ok = store_->save(next);
  if (ok) {
    *config_ = next;
    updateCredentials(next.security);
    if (saveHandler_) saveHandler_(next, saveCtx_);
  }
```

- [ ] **Step 9: Verify no remaining `basicAuth` references in the web layer**

Run: `grep -n "basicAuth\|authenticate(" shared/src/web/web_server.cpp shared/src/web/web_server.h`

Expected: No `basicAuth` matches anywhere. No `authenticate(` matches in either file (the library member function `request->authenticate(...)` was only used inside the deleted helper and is gone with it).

- [ ] **Step 10: Run all `web_server_auth.test.mjs` assertions**

Run: `node --test tests/web_server_auth.test.mjs`

Expected: All tests PASS (Task 1's two tests + Task 2's seven tests = nine total).

- [ ] **Step 11: Sync shared tree**

Run: `tools/sync_shared.sh`

Expected: Script completes silently. Both `firmware/*-lite/src/web/web_server.{h,cpp}` now mirror the shared tree.

- [ ] **Step 12: Commit**

```bash
git add shared/src/web/web_server.h shared/src/web/web_server.cpp \
        firmware/atom-lite/src/web/web_server.h firmware/atom-lite/src/web/web_server.cpp \
        firmware/atoms3-lite/src/web/web_server.h firmware/atoms3-lite/src/web/web_server.cpp \
        tests/web_server_auth.test.mjs
git -c commit.gpgsign=false commit -m "feat(web): enforce auth via AsyncAuthenticationMiddleware

Attach a single AsyncAuthenticationMiddleware globally to both the
HTTP server and /ws/console WebSocket so every route (static admin
UI, /api/*, /update OTA, WebSocket handshake) requires HTTP Basic
Auth. Hot-swap credentials when /api/config saves a new password.
Remove the per-route authenticate() helper and its call sites."
```

---

## Task 3: Remove `basic_auth` field from admin UI

**Files:**
- Modify: `shared/data/index.html` (lines 1196, 1345, 1498, 1626, 1748)
- Modify: `tests/web_ui_index.test.mjs`

- [ ] **Step 1: Extend `tests/web_ui_index.test.mjs` with the failing assertion**

Open `tests/web_ui_index.test.mjs`. Inside the existing test scope (after the other `assert.doesNotMatch` lines near the top of the file), add:

```javascript
assert.doesNotMatch(html, /name="basic_auth"/);
assert.doesNotMatch(html, /id="cfgBasicAuth"/);
assert.doesNotMatch(html, /basic_auth\s*:/);
```

- [ ] **Step 2: Run the test to confirm it fails**

Run: `node --test tests/web_ui_index.test.mjs`

Expected: FAIL with mismatches (current HTML still contains all three substrings).

- [ ] **Step 3: Delete the `<select>` for Basic Auth**

In `shared/data/index.html`, delete line 1196 entirely:

```html
                <div class="field"><label for="cfgBasicAuth">Basic Auth</label><select id="cfgBasicAuth" name="basic_auth"><option value="enabled">활성</option><option value="disabled">비활성</option></select></div>
```

The line should be removed in full, including its preceding whitespace.

- [ ] **Step 4: Remove `basic_auth` from JS default state**

In `shared/data/index.html` around line 1345, before:

```javascript
      security: { basic_auth: true, username: "admin" },
```

After:

```javascript
      security: { username: "admin" },
```

- [ ] **Step 5: Remove `basic_auth` from POST payload**

In `shared/data/index.html` around line 1498, locate the payload construction in the settings save handler. Before:

```javascript
          basic_auth: data.basic_auth,
```

Delete this line entirely. Verify the resulting object literal still has correct comma placement.

- [ ] **Step 6: Remove `basic_auth` UI hydration**

In `shared/data/index.html` around line 1626, before:

```javascript
      $("#cfgBasicAuth").value = state.security.basic_auth === false ? "disabled" : "enabled";
```

Delete this line entirely.

- [ ] **Step 7: Remove `basic_auth` from state merge**

In `shared/data/index.html` around line 1748, before:

```javascript
      mergeDefined(state.security, { basic_auth: payload.basic_auth !== "disabled" });
```

After:

```javascript
      mergeDefined(state.security, {});
```

(If the resulting `mergeDefined(state.security, {})` line is now a no-op, you may delete it entirely instead. Confirm by reading the surrounding lines — leave the line out only if removing it does not break a chain of related state merges.)

- [ ] **Step 8: Verify no remaining `basic_auth` references in the UI**

Run: `grep -n "basic_auth\|cfgBasicAuth\|BasicAuth" shared/data/index.html`

Expected: No matches.

- [ ] **Step 9: Run the UI test**

Run: `node --test tests/web_ui_index.test.mjs`

Expected: PASS (including the three new `doesNotMatch` assertions). Version pill assertion still fails — that is fixed in Task 6.

- [ ] **Step 10: Sync shared tree**

Run: `tools/sync_shared.sh`

Expected: Both `firmware/*-lite/data/index.html` updated, and `admin_ui.h` regenerated under `shared/src/web/` and both board trees.

- [ ] **Step 11: Commit**

```bash
git add shared/data/index.html \
        firmware/atom-lite/data/index.html firmware/atom-lite/src/web/admin_ui.h \
        firmware/atoms3-lite/data/index.html firmware/atoms3-lite/src/web/admin_ui.h \
        shared/src/web/admin_ui.h \
        tests/web_ui_index.test.mjs
git -c commit.gpgsign=false commit -m "feat(ui): remove basic_auth toggle from admin UI

Authentication is mandatory in 0.1.9; the toggle is gone from the
SecurityConfig and from the admin UI form, default state, payload,
hydration, and merge paths."
```

---

## Task 4: Update security documentation

**Files:**
- Modify: `docs/security.md`
- Modify: `docs/security.en.md`

- [ ] **Step 1: Read the current Korean security doc**

Run: `wc -l docs/security.md && head -80 docs/security.md`

Note the section structure. Identify the section that previously described "Basic Auth Scope Trade-off" or any text saying that Basic Auth may be off.

- [ ] **Step 2: Replace the auth-scope language in `docs/security.md`**

Replace the relevant section with this Korean text (insert under the closest existing heading such as "인증 및 권한" or create one if absent):

```markdown
## 웹 인증 정책

0.1.9부터 모든 웹 접근에 HTTP Basic Auth가 강제됩니다. 적용 범위는 다음과 같습니다.

- 정적 어드민 UI(`/`, `/dashboard`, `/console`, `/ota`, `/about`, 기타 미정의 경로)
- 모든 `/api/*` 엔드포인트(읽기/쓰기 무관)
- OTA 펌웨어 업로드(`/update`)
- WebSocket(`/ws/console`) 핸드셰이크

인증되지 않은 요청은 `401 Unauthorized`와 `WWW-Authenticate: Basic realm="DM-D5102Q Bridge"` 헤더를 받습니다.

기본 자격증명은 NVS에 저장된 값 → `secrets.h`의 `BRIDGE_DEFAULT_WEB_USER` / `BRIDGE_DEFAULT_WEB_PASSWORD` → 컴파일 기본값 `admin/admin` 순서로 적용됩니다. 첫 부팅 후 즉시 비밀번호를 변경하세요.

자격증명을 잊으면 부팅 시 버튼 5초 홀드로 설정만 초기화하거나(WiFi 정보 유지) 8초 이상 홀드로 NVS 전체 초기화로 복구합니다.
```

If the file previously had a "Basic Auth Scope Trade-off" section, delete that block entirely and replace it with the text above.

- [ ] **Step 3: Replace the auth-scope language in `docs/security.en.md`**

Apply the equivalent English text:

```markdown
## Web Authentication Policy

Starting with 0.1.9, every web access requires HTTP Basic Auth. The gate covers:

- Static admin UI (`/`, `/dashboard`, `/console`, `/ota`, `/about`, and any other undefined GET path)
- All `/api/*` endpoints (read and write alike)
- OTA firmware upload (`/update`)
- WebSocket (`/ws/console`) handshake

Unauthenticated requests receive `401 Unauthorized` with the header `WWW-Authenticate: Basic realm="DM-D5102Q Bridge"`.

Default credentials resolve in this order: value stored in NVS → `BRIDGE_DEFAULT_WEB_USER` / `BRIDGE_DEFAULT_WEB_PASSWORD` macros in `secrets.h` → compiled defaults `admin` / `admin`. Change the password immediately after first boot.

If credentials are lost, hold the boot button for 5 seconds to wipe settings only (WiFi credentials preserved) or 8+ seconds for a full NVS wipe.
```

- [ ] **Step 4: Verify both files mention the new policy**

Run: `grep -n "0.1.9\|WWW-Authenticate\|Basic Auth" docs/security.md docs/security.en.md`

Expected: Each file shows at least the introductory line and the WWW-Authenticate header.

- [ ] **Step 5: Commit**

```bash
git add docs/security.md docs/security.en.md
git -c commit.gpgsign=false commit -m "docs(security): mandatory web auth across every surface

Document that 0.1.9 forces HTTP Basic Auth on static HTML, /api/*,
OTA upload, and the WebSocket handshake. Spell out credential
resolution order and recovery via factory reset."
```

---

## Task 5: Update HTTP API documentation

**Files:**
- Modify: `docs/api/http.md`
- Modify: `docs/api/http.en.md`

- [ ] **Step 1: Inspect the current endpoint table**

Run: `head -120 docs/api/http.md`

Identify the endpoint table (each row likely lists method, path, description, body schema, etc.). Note whether an "Auth Required" column already exists.

- [ ] **Step 2: Add or update the "인증 필요" column in `docs/api/http.md`**

For every endpoint row in the Korean doc:

- `GET /api/status` → 인증 필요: ✓
- `GET /api/config` → 인증 필요: ✓
- `POST /api/config` → 인증 필요: ✓
- `GET /api/wifi/scan` → 인증 필요: ✓
- `POST /api/tx` → 인증 필요: ✓
- `GET /api/scanner/result` → 인증 필요: ✓
- `POST /api/scanner/start` → 인증 필요: ✓
- `POST /api/scanner/stop` → 인증 필요: ✓
- `POST /api/factory-reset/settings` → 인증 필요: ✓
- `POST /api/factory-reset/all` → 인증 필요: ✓
- `POST /update` → 인증 필요: ✓

If a column does not exist, add a new column at the right edge of the table. If the doc uses a per-endpoint subsection instead of a table, add a "인증 필요: ✓" line under each endpoint heading.

- [ ] **Step 3: Document `401 Unauthorized` and drop `basic_auth` parameter**

Find the section describing request body parameters for `POST /api/config`. Remove any row or bullet that describes `basic_auth` (e.g. "`basic_auth` (string, `enabled`|`disabled`)").

Append the following to a "공통 에러" or "공통 응답" section in `docs/api/http.md`:

```markdown
### 401 Unauthorized

인증 헤더가 없거나 잘못된 경우 반환됩니다.

응답 헤더: `WWW-Authenticate: Basic realm="DM-D5102Q Bridge"`

응답 본문 형식은 ESPAsyncWebServer 기본값(`Authentication required`)을 따릅니다.
```

- [ ] **Step 4: Apply the equivalent changes to `docs/api/http.en.md`**

Repeat steps 2 and 3 in English. Column header: "Auth Required". Common error block:

```markdown
### 401 Unauthorized

Returned when the request lacks valid Basic Auth credentials.

Response header: `WWW-Authenticate: Basic realm="DM-D5102Q Bridge"`

The body matches the ESPAsyncWebServer default message (`Authentication required`).
```

Also remove the `basic_auth` parameter row for `POST /api/config`.

- [ ] **Step 5: Verify**

Run: `grep -n "basic_auth" docs/api/http.md docs/api/http.en.md`

Expected: No matches.

Run: `grep -n "WWW-Authenticate" docs/api/http.md docs/api/http.en.md`

Expected: At least one match per file.

- [ ] **Step 6: Commit**

```bash
git add docs/api/http.md docs/api/http.en.md
git -c commit.gpgsign=false commit -m "docs(api): mark every HTTP endpoint as auth-required

Add the auth-required marker to every endpoint, document the 401
Unauthorized response with WWW-Authenticate header, and drop the
removed basic_auth parameter from POST /api/config."
```

---

## Task 6: Update getting-started, README, and CHANGELOG; bump version to 0.1.9

**Files:**
- Modify: `docs/getting-started.md`
- Modify: `docs/getting-started.en.md`
- Modify: `README.md:8` (version line)
- Modify: `README.en.md` (version line — search for `0.1.8`)
- Modify: `CHANGELOG.md`
- Modify: `CHANGELOG.en.md`
- Modify: `shared/data/index.html:1030, 1346`
- Modify: `shared/src/app/firmware_app.cpp:208`
- Modify: `shared/src/status/device_status.cpp:91`
- Modify: `tests/web_ui_index.test.mjs` (version assertion if it pins to `0.1.8`)

- [ ] **Step 1: Add the first-login walk-through to `docs/getting-started.md`**

Open `docs/getting-started.md`. After the section explaining how to power on the device and discover the AP, add a new subsection (preserve the existing heading style):

```markdown
### 첫 로그인

0.1.9부터 모든 웹 접근은 HTTP Basic Auth로 보호됩니다.

1. AP 모드에서 `http://192.168.4.1/`에 접속하면 브라우저가 즉시 인증 팝업을 표시합니다(`realm: "DM-D5102Q Bridge"`).
2. 기본 자격증명은 사용자 이름 `admin`, 비밀번호 `admin`입니다. `secrets.h`에서 `BRIDGE_DEFAULT_WEB_USER` / `BRIDGE_DEFAULT_WEB_PASSWORD`를 정의한 경우 그 값을 사용합니다.
3. 로그인 후 어드민 UI > 설정 > 보안 섹션에서 비밀번호를 즉시 변경하세요.
4. STA 모드로 전환되면 새 IP에서 다시 인증을 요구합니다.
```

- [ ] **Step 2: Add the same walk-through to `docs/getting-started.en.md`**

```markdown
### First login

Starting with 0.1.9, every web access is protected with HTTP Basic Auth.

1. After connecting to the AP, opening `http://192.168.4.1/` triggers a browser auth prompt (`realm: "DM-D5102Q Bridge"`).
2. Default credentials are username `admin`, password `admin`. If `secrets.h` defines `BRIDGE_DEFAULT_WEB_USER` and `BRIDGE_DEFAULT_WEB_PASSWORD`, those values apply instead.
3. After logging in, change the password immediately in the Admin UI > Settings > Security section.
4. After switching to STA mode, the new IP will prompt for credentials again.
```

- [ ] **Step 3: Update `README.md` version line**

Replace line 8 of `README.md`:

Before:

```markdown
- 현재 버전: 0.1.8 (Unreleased)
```

After:

```markdown
- 현재 버전: 0.1.9 (Unreleased)
```

- [ ] **Step 4: Update `README.en.md` version line**

Search `README.en.md` for `0.1.8` and replace with `0.1.9` (typical match: a `Current version: 0.1.8 (Unreleased)` line near the top).

Run: `grep -n "0.1.8\|0.1.9" README.en.md`

Expected after edit: Only `0.1.9` matches remain on the version line.

- [ ] **Step 5: Bump version in `shared/data/index.html`**

In `shared/data/index.html`, change line 1030:

Before:

```html
              <span class="pill" data-field="version">0.1.8</span>
```

After:

```html
              <span class="pill" data-field="version">0.1.9</span>
```

Change line 1346:

Before:

```javascript
      device: { name: "bridge", board: "M5Stack AtomS3 Lite", version: "0.1.8", build: "local" },
```

After:

```javascript
      device: { name: "bridge", board: "M5Stack AtomS3 Lite", version: "0.1.9", build: "local" },
```

- [ ] **Step 6: Bump firmware boot log**

In `shared/src/app/firmware_app.cpp` line 208:

Before:

```cpp
  log_.log("[boot] dm-d5102q-bridge v0.1.8 build=%s %s", __DATE__, __TIME__);
```

After:

```cpp
  log_.log("[boot] dm-d5102q-bridge v0.1.9 build=%s %s", __DATE__, __TIME__);
```

- [ ] **Step 7: Bump JSON device.version**

In `shared/src/status/device_status.cpp` line 91:

Before:

```cpp
  device["version"] = "0.1.8";
```

After:

```cpp
  device["version"] = "0.1.9";
```

- [ ] **Step 8: Update version assertion in `tests/web_ui_index.test.mjs`**

Run: `grep -n "0.1.8\|0.1.9" tests/web_ui_index.test.mjs`

If a `0.1.8` string appears (likely an assertion such as `assert.match(html, /data-field="version">0\.1\.8</)`), update it to `0.1.9`. If both substrings match, ensure the assertion now pins to `0.1.9`.

- [ ] **Step 9: Add the 0.1.9 entry to `CHANGELOG.md`**

Open `CHANGELOG.md`. Above the most recent existing entry (likely `## 0.1.8`), insert:

```markdown
## 0.1.9 - 2026-05-27

### Changed
- 웹 어드민과 프로비저닝 화면이 모든 경로(정적 HTML, `/api/*`, `/update` OTA, `/ws/console` WebSocket)에서 HTTP Basic Auth를 강제합니다.
- 설정 스키마와 어드민 UI에서 `basic_auth` 토글 필드를 제거했습니다. 인증은 항상 적용됩니다.

### Security
- `/update` OTA 업로드 엔드포인트가 이제 인증을 요구합니다(이전에는 자격증명 없이 접근 가능).
- `/ws/console` WebSocket 핸드셰이크가 이제 인증을 요구합니다.
- 모든 GET 엔드포인트(`/api/status`, `/api/config`, `/api/wifi/scan`, `/api/scanner/result`)가 이제 인증을 요구합니다.
```

- [ ] **Step 10: Add the 0.1.9 entry to `CHANGELOG.en.md`**

```markdown
## 0.1.9 - 2026-05-27

### Changed
- Web admin and provisioning screens now require HTTP Basic Auth on every route (static HTML, `/api/*`, `/update` OTA, `/ws/console` WebSocket).
- Removed the `basic_auth` toggle field from the configuration schema and the admin UI — authentication is always enforced.

### Security
- `/update` OTA upload endpoint is now authenticated; previously accessible without credentials.
- `/ws/console` WebSocket handshake is now authenticated.
- All GET endpoints (`/api/status`, `/api/config`, `/api/wifi/scan`, `/api/scanner/result`) now require credentials.
```

- [ ] **Step 11: Verify no `0.1.8` stragglers**

Run: `grep -rn "0\.1\.8" shared/ tests/ docs/ README.md README.en.md CHANGELOG.md CHANGELOG.en.md | grep -v "^docs/superpowers/" | grep -v "graphify-out/"`

Expected: Only historical references inside `CHANGELOG.md` / `CHANGELOG.en.md` under the existing `## 0.1.8` section remain. No references in `shared/`, `tests/`, `README.*`, `docs/getting-started.*`, `docs/security.*`, or `docs/api/`.

- [ ] **Step 12: Sync shared tree**

Run: `tools/sync_shared.sh`

Expected: Both firmware trees inherit the new version strings. `admin_ui.h` regenerated.

- [ ] **Step 13: Run all tests**

Run: `node --test tests/`

Expected: All three test files (`firmware_structure.test.mjs`, `web_ui_index.test.mjs`, `web_server_auth.test.mjs`) PASS with no failures.

- [ ] **Step 14: Commit**

```bash
git add docs/getting-started.md docs/getting-started.en.md \
        README.md README.en.md \
        CHANGELOG.md CHANGELOG.en.md \
        shared/data/index.html shared/src/app/firmware_app.cpp shared/src/status/device_status.cpp \
        firmware/atom-lite/data/index.html firmware/atom-lite/src/app/firmware_app.cpp firmware/atom-lite/src/status/device_status.cpp firmware/atom-lite/src/web/admin_ui.h \
        firmware/atoms3-lite/data/index.html firmware/atoms3-lite/src/app/firmware_app.cpp firmware/atoms3-lite/src/status/device_status.cpp firmware/atoms3-lite/src/web/admin_ui.h \
        shared/src/web/admin_ui.h \
        tests/web_ui_index.test.mjs
git -c commit.gpgsign=false commit -m "chore: bump firmware to 0.1.9 with first-login docs

Document the AP-mode first-login flow with default credentials in
getting-started (KO/EN), update README and CHANGELOG, and bump
every version string (boot log, JSON device.version, admin UI
pill, JS default state, version assertion) to 0.1.9."
```

---

## Task 7: Compile both boards and produce the OTA artifact

**Files:**
- Output: `releases/dm-d5102q-bridge-0.1.9-atoms3-lite-web-auth-gate.bin`
- Output (optional): `releases/dm-d5102q-bridge-0.1.9-atom-lite-web-auth-gate.bin`

- [ ] **Step 1: Confirm Arduino CLI environment**

Run: `arduino-cli core list | grep -i m5stack`

Expected: `m5stack:esp32` core listed. If absent, install via `arduino-cli core install m5stack:esp32` first.

- [ ] **Step 2: Compile the AtomS3-Lite target**

Run from the project root:

```bash
arduino-cli compile \
  --fqbn m5stack:esp32:m5stack_atoms3 \
  --warnings default \
  --build-path firmware/atoms3-lite/build \
  firmware/atoms3-lite
```

Expected: Compilation succeeds. Output ends with a line such as `Used PROGRAM memory: ... bytes (...% used)`.

If compile fails on `AsyncAuthenticationMiddleware` symbol resolution, the local ESPAsyncWebServer fork is the wrong version — install the ESP32Async fork. Verify with `arduino-cli lib list | grep -i ESPAsyncWebServer`.

- [ ] **Step 3: Compile the Atom-Lite target**

```bash
arduino-cli compile \
  --fqbn m5stack:esp32:m5stack_atom \
  --warnings default \
  --build-path firmware/atom-lite/build \
  firmware/atom-lite
```

Expected: Compilation succeeds.

- [ ] **Step 4: Package the OTA artifact for AtomS3-Lite**

```bash
mkdir -p releases
cp firmware/atoms3-lite/build/atoms3-lite.ino.bin \
   releases/dm-d5102q-bridge-0.1.9-atoms3-lite-web-auth-gate.bin
ls -lh releases/dm-d5102q-bridge-0.1.9-atoms3-lite-web-auth-gate.bin
```

Expected: File exists, size in the typical 1.0–1.5 MB range.

- [ ] **Step 5: Package the OTA artifact for Atom-Lite (optional)**

```bash
cp firmware/atom-lite/build/atom-lite.ino.bin \
   releases/dm-d5102q-bridge-0.1.9-atom-lite-web-auth-gate.bin
ls -lh releases/dm-d5102q-bridge-0.1.9-atom-lite-web-auth-gate.bin
```

Expected: File exists.

- [ ] **Step 6: Smoke test via grep — no version stragglers in the produced binaries**

Run: `strings releases/dm-d5102q-bridge-0.1.9-atoms3-lite-web-auth-gate.bin | grep -E "0\.1\.[0-9]+" | sort -u`

Expected: Only `0.1.9` appears (with possible substring matches inside library version strings unrelated to this firmware version — verify only the user-visible `dm-d5102q-bridge v0.1.9` is present and no `v0.1.8`).

- [ ] **Step 7: Hand the artifact path to the user**

Present this confirmation message:

> Firmware 0.1.9 OTA artifacts are ready:
> - `releases/dm-d5102q-bridge-0.1.9-atoms3-lite-web-auth-gate.bin`
> - `releases/dm-d5102q-bridge-0.1.9-atom-lite-web-auth-gate.bin` (if Atom-Lite was compiled)
>
> Upload via the admin UI's OTA page (after logging in with the existing pre-0.1.9 credentials).
> After reboot the device requires the new auth flow — see the manual verification checklist below.

- [ ] **Step 8: Print the manual verification checklist for the user to run**

After the user OTA-uploads and the device reboots, instruct them to walk through the 14-item checklist from `docs/superpowers/specs/2026-05-27-web-admin-auth-gate-design.md` section 8.4. Reproduce the checklist verbatim here for convenience:

1. AP-mode first connect: `http://192.168.4.1/` shows a Basic Auth popup.
2. Wrong credentials → 401, popup re-appears.
3. Correct credentials → admin UI loads.
4. Save WiFi → reboot into STA → STA IP requires credentials again.
5. Each page (Dashboard / Console / OTA / About) without credentials → 401.
6. `curl http://host/api/status` (no creds) → 401.
7. `curl -u admin:admin http://host/api/status` → 200 JSON.
8. `curl -X POST http://host/update -F "data=@firmware.bin"` (no creds) → 401, no flash write.
9. `wscat -c ws://host/ws/console` (no creds) → handshake 401.
10. Browser admin UI logged in → WebSocket packets stream normally on `/console`.
11. Change password to `newpass` → next page navigation prompts for new credentials.
12. 5-second boot button hold → factory reset (settings only) → credentials revert, WiFi retained.
13. OTA upload (authenticated) → succeeds → device reboots into the new image.
14. OTA from a pre-0.1.9 image: NVS contains old `basic_auth` key → new firmware boots, credentials work, no warning required.

- [ ] **Step 9: Do not commit the release binaries**

Binaries do not enter git. Add `releases/` to `.gitignore` if it is not already ignored:

Run: `grep -n "^releases" .gitignore 2>/dev/null`

If no match, run:

```bash
printf "\n# release artifacts\nreleases/\n" >> .gitignore
git add .gitignore
git -c commit.gpgsign=false commit -m "chore: ignore release artifact directory"
```

If `releases/` is already in `.gitignore`, skip the commit for this step.

---

## Self-Review (run after all tasks above are checked off)

- [ ] **Spec coverage:** Every section of `docs/superpowers/specs/2026-05-27-web-admin-auth-gate-design.md` has a corresponding task in this plan.
  - §2 Decisions → locked into Task 2 (middleware attach + Basic Auth) and Task 3 (toggle removal)
  - §3 Architecture → Task 2 Step 4
  - §4.1–4.5 Component changes → Tasks 1, 2
  - §4.6 Admin UI changes → Task 3
  - §4.8 Documentation → Tasks 4, 5, 6
  - §4.9 Version bump → Task 6
  - §5 Credential lifecycle → Task 2 Step 8 (hot-swap)
  - §6 Edge cases → Task 7 Step 8 manual checklist
  - §7 CHANGELOG entry → Task 6 Steps 9–10
  - §8 Tests → Tasks 1, 2, 3, 6 Step 13
  - §9 Commit plan → seven commits across Tasks 1–7 (matches one-to-one)
- [ ] **No placeholders:** No "TBD" / "TODO" / "add validation" / "similar to Task N" markers exist in this plan.
- [ ] **Type consistency:** `updateCredentials(const SecurityConfig&)` is declared in `web_server.h` (Task 2 Step 3) and defined in `web_server.cpp` (Task 2 Step 7) with the same signature; it is called from `handleConfigBody` in Task 2 Step 8 with `next.security` which has the matching type. `AsyncAuthenticationMiddleware authMiddleware_` is the single source of truth used in Task 2 Steps 3, 4, 7.

---

## Execution Handoff

Plan complete and saved to `docs/superpowers/plans/2026-05-27-web-admin-auth-gate.md`. Two execution options:

1. **Subagent-Driven (recommended)** — fresh subagent per task with two-stage review between tasks. Best when each task should be independently verified before the next starts.
2. **Inline Execution** — execute tasks in this session sequentially with checkpoints at each task boundary.

Which approach?
