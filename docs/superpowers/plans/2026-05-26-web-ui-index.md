# Web UI Index Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a firmware-friendly `shared/data/index.html` that recreates the handoff UI and covers the PRD web console, dashboard, settings, provisioning, OTA, and diagnostic surfaces.

**Architecture:** Use one self-contained HTML file with embedded CSS and vanilla JavaScript so LittleFS can serve it without CDN, React, Babel, or a build step. The UI talks to draft firmware endpoints when available and falls back to deterministic demo state for local review.

**Tech Stack:** Static HTML5, CSS custom properties, vanilla JavaScript, WebSocket `/ws/console`, HTTP API drafts under `/api/*`, Node built-in test script.

---

### Task 1: Web UI Validation Harness

**Files:**
- Create: `tests/web_ui_index.test.mjs`
- Create: `docs/superpowers/plans/2026-05-26-web-ui-index.md`

- [ ] **Step 1: Write the failing test**

```js
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const html = readFileSync(join(root, "shared/data/index.html"), "utf8");

assert.match(html, /<title>DM-D5102Q RS485 TCP Bridge<\/title>/);
assert.doesNotMatch(html, /https?:\/\//);
assert.match(html, /id="app"/);
assert.match(html, /data-route="dashboard"/);
assert.match(html, /data-route="console"/);
assert.match(html, /data-route="settings"/);
assert.match(html, /data-route="provisioning"/);
assert.match(html, /data-route="ota"/);
assert.match(html, /\/ws\/console/);
assert.match(html, /\/api\/status/);
assert.match(html, /\/api\/config/);
assert.match(html, /\/api\/tx/);
assert.match(html, /\/update/);
assert.match(html, /MAX_CONSOLE_LINES\s*=\s*300/);
assert.match(html, /MAX_TX_BYTES\s*=\s*256/);
assert.match(html, /function parseHexInput/);
assert.match(html, /function addConsolePacket/);
assert.match(html, /function saveConfig/);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `node tests/web_ui_index.test.mjs`

Expected: FAIL because `shared/data/index.html` does not exist yet.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/plans/2026-05-26-web-ui-index.md tests/web_ui_index.test.mjs
git commit -m "test: add web ui index validation"
```

### Task 2: Self-Contained Firmware Index

**Files:**
- Create: `shared/data/index.html`
- Modify: `tests/web_ui_index.test.mjs`

- [ ] **Step 1: Implement `shared/data/index.html`**

The file must embed CSS and JavaScript directly, expose hash routes for dashboard, console, settings, provisioning, OTA, and about views, and keep console DOM lines bounded by `MAX_CONSOLE_LINES = 300`.

- [ ] **Step 2: Run validation**

Run: `node tests/web_ui_index.test.mjs`

Expected: PASS with all structural checks satisfied.

- [ ] **Step 3: Commit**

```bash
git add shared/data/index.html tests/web_ui_index.test.mjs
git commit -m "feat: implement firmware web ui index"
```

### Task 3: Browser Smoke Verification

**Files:**
- Modify only if browser smoke reveals a layout or runtime issue.

- [ ] **Step 1: Serve static assets**

Run: `python3 -m http.server 8000 --directory shared/data`

Expected: server starts and serves `index.html`.

- [ ] **Step 2: Open browser and verify**

Open `http://127.0.0.1:8000`. Verify dashboard renders, navigation works, console accepts valid HEX, invalid HEX is rejected, and settings/provisioning/OTA pages are reachable.

- [ ] **Step 3: Commit fixes if needed**

```bash
git add shared/data/index.html
git commit -m "fix: polish web ui browser smoke issues"
```

## Self-Review

- PRD coverage: dashboard metrics, Wi-Fi provisioning, UART/TCP settings, RS485 HEX TX/RX console, bounded console buffer, WebSocket stream endpoint, OTA upload path, overflow counters, and factory reset guidance are covered by Task 2.
- Placeholder scan: no implementation placeholder is allowed in `index.html`; unavailable firmware APIs must use explicit demo fallback, not TODO copy.
- Type consistency: config fields in JavaScript use stable string keys matching UI form names and API payloads.
