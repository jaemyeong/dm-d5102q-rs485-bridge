# Web Admin Header Redesign — Design Spec

- Status: Draft
- Date: 2026-05-29
- Author: Jaemyeong Jin
- Target firmware version: 0.1.10
- Scope: Redesign the persistent admin header into a hybrid (46px summary + click-to-expand drawer) surface that shows device health at a glance, surfaces operational alerts, exposes a reboot action, and adds the supporting backend API contract.

## 1. Problem

The current header (`shared/data/index.html` L900-919) is a single 46px row split brand · nav · top-state. The top-state slot holds the LED chip, IP, RSSI, and uptime. Operationally this fails three goals:

- **No health summary** — the LED chip mirrors the physical LED state, not application health. Auth failures, queue overflows, RX stalls, Wi-Fi loss are invisible from the header. Operators must navigate to dashboard or console to discover problems.
- **No top-level action** — destructive operations (reboot, factory reset, OTA) live inside sub-pages. A field technician must navigate three menus to reboot a misbehaving device.
- **No alert mechanism** — `DeviceStatus` already tracks overflow / dropped / rejected counters, but they never propagate to the header. The bus quietly degrades while the header looks normal.

The goal: turn the header into a control surface that answers four questions at a glance — *Is everything normal? How is the network? Is data flowing? Are there alerts?* — and provides one always-visible action (reboot) protected by a confirmation modal.

## 2. Decisions (locked in)

| # | Decision | Choice | Rationale |
|---|---|---|---|
| D1 | Layout strategy | **Hybrid** — 46px topbar + click-expandable drawer | Preserves vertical density on every page while allowing detail on demand; chosen via ASCII mockup comparison vs Compact and Tall 2-row |
| D2 | Always-visible info | Health pill (`● 정상` / `⚠ 경고` / `✕ 장애`) + alert count badge + reboot button | Matches the user-stated "answer 4 questions" goals while keeping the cluster scannable in <140px |
| D3 | Reboot UX | Click → confirmation modal → 200 OK → modal transitions to "재부팅 중..." → polling reload | Standard web pattern with explicit destructive confirmation; rejected hold-to-confirm and swipe-to-reboot for simplicity |
| D4 | Post-reboot reconnect | Client polls `GET /api/status` once per second until 200; 90s timeout falls back to manual `[다시 시도]` | Adaptive to actual recovery time; tolerates Wi-Fi reconnection variance |
| D5 | Drawer trigger | Right-side summary cluster (excluding reboot button) is the toggle hit area | Reboot must be a single-purpose hit; the rest of the cluster naturally invites click |
| D6 | Drawer state persistence | `sessionStorage` — open across route navigation, closed on page reload | Lets operators keep detail open while flipping tabs; reload acts as a clean reset |
| D7 | Drawer dismiss | Re-click summary OR ✕ button OR Esc OR outside click | Multi-modal for keyboard and pointer users |
| D8 | Health computation | Worst-wins among (subsystem state + active alerts); 3 levels + offline | Single composite pill is easier to read than per-subsystem chips |
| D9 | Alert severity model | warning + critical, level-triggered or edge-triggered with TTL | level for ongoing conditions (Wi-Fi loss), edge for bursts (overflow); TTL keeps the panel clean |
| D10 | Icon strategy | Unicode glyphs only (`●▲▼✓✕⚠⏻⊘⟳⇄▮▯◉○▾▴`); no emoji | Matches existing admin UI tone; consistent rendering across platforms |
| D11 | Theme | Dark single-theme | Existing UI has no light variant; out of scope to add one now |
| D12 | Mobile (≤640px) | Hamburger nav, reboot label hidden (⏻ only), drawer becomes a single column stack | Maintains all five header capabilities on small screens |
| D13 | Rate calculation | Client-side derivation from successive `/api/status` samples | Avoids server-side per-second buffer; simpler firmware; multi-client UI consistency is not required (single-operator surface) |
| D14 | New backend fields | `metrics.freeHeap`, `metrics.authFailures`, `metrics.lastAuthFailMs` | Required for memory alert, auth failure alert, and Device drawer panel |
| D15 | New backend endpoints | `GET /api/info`, `POST /api/reboot` | `/api/info` carries static meta (version, board, capacities); `/api/reboot` exposes the action |
| D16 | Build-time metadata | `BUILD_COMMIT`, `BUILD_AT` injected via PlatformIO `extra_scripts` | Lets `/api/info` report exact built artifact for OTA verification |
| D17 | Auth wiring | Existing global `AsyncAuthenticationMiddleware` (0.1.9) protects all new endpoints | Zero new auth surface; reuses the locked policy |
| D18 | Version | **0.1.10** (CHANGELOG, package manifest, platformio sync) | Patch bump — additive feature, no breaking API |

## 3. Architecture

### 3.1 Component diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│  shared/data/index.html  (served PROGMEM gzip — 0.1.2 pattern)        │
│                                                                       │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │  <header class="topbar">  (46px fixed)                          │  │
│  │    <div.brand>  <nav.nav-tabs>  <div.summary [role=button]>    │  │
│  │       ▲ click toggle on .summary                                │  │
│  │       ▲ click independent on #rebootBtn                         │  │
│  └────────────────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │  <div id="header-drawer" [role=region]>                         │  │
│  │    Network panel · Traffic panel · Device panel (3 col desktop) │  │
│  │    Alerts area [role=status aria-live=polite]                   │  │
│  └────────────────────────────────────────────────────────────────┘  │
│  <dialog id="rebootDialog" aria-modal="true">  ← reboot confirm       │
│                                                                       │
│  <script>                                                             │
│    deriveRates / computeHealth / formatUptime / wrapDelta  (testable)│
│    drawer toggle, modal flow, polling loop, alert lifecycle          │
│  </script>                                                            │
└──────────────────────────────────────────────────────────────────────┘
              ▲                                          ▲
              │ WebSocket /ws/console (1Hz)              │ POST /api/reboot
              │ GET /api/status (fallback / poll)        │ GET /api/info (1x)
              ▼                                          ▼
┌──────────────────────────────────────────────────────────────────────┐
│  WebServer  (shared/src/web/web_server.cpp)                           │
│    server.addMiddleware(authMw)  ← 0.1.9 global gate                  │
│    server.on("/api/status",  HTTP_GET, …)   ← extended JSON           │
│    server.on("/api/info",    HTTP_GET, …)   ← NEW                     │
│    server.on("/api/reboot",  HTTP_POST,…)   ← NEW                     │
│    rebootScheduledMs_   ← polled by FirmwareApp                       │
│    authMw failure → status_->recordAuthFailure()                      │
└──────────────────────────────────────────────────────────────────────┘
                                  ▲
                                  │
┌──────────────────────────────────────────────────────────────────────┐
│  DeviceStatus  (shared/src/status/device_status.{h,cpp})              │
│    + uint32_t authFailures = 0                                        │
│    + uint32_t lastAuthFailMs = 0                                      │
│    + void recordAuthFailure()                                         │
│    JSON: existing + freeHeap(=ESP.getFreeHeap()) + authFailures*       │
└──────────────────────────────────────────────────────────────────────┘
                                  ▲
                                  │
┌──────────────────────────────────────────────────────────────────────┐
│  FirmwareApp  (shared/src/app/firmware_app.cpp)                       │
│    poll() { …existing…; web_.pollRebootDeadline(); }                  │
│      ↳ web_.pollRebootDeadline() calls ESP.restart() at deadline      │
└──────────────────────────────────────────────────────────────────────┘
```

3-way mirror policy (master in `shared/`, mirrored to `firmware/atom-lite/src/` and `firmware/atoms3-lite/src/`) is preserved for every changed firmware file.

### 3.2 Header DOM skeleton

```html
<header class="topbar">
  <div class="brand">
    <span class="brand-mark"></span>
    <span>DM-D5102Q</span>
  </div>
  <nav class="nav-tabs" aria-label="주요 메뉴">
    <button data-route="dashboard" class="active">대시보드</button>
    <button data-route="console">콘솔</button>
    <!-- … existing tabs … -->
  </nav>
  <div class="summary" role="button" tabindex="0"
       aria-expanded="false" aria-controls="header-drawer"
       aria-label="장치 상태 요약, 펼치려면 활성화">
    <span id="navHealth" class="health-pill health-ok">
      <span class="health-glyph" aria-hidden="true">●</span>
      <span class="health-label">정상</span>
    </span>
    <span id="navAlertCount" class="alert-badge alert-badge-zero" aria-live="polite">
      <span aria-hidden="true">⚠</span> 0
    </span>
    <button id="rebootBtn" type="button" class="reboot-btn"
            aria-label="장치 재부팅" aria-haspopup="dialog">
      <span class="reboot-glyph" aria-hidden="true">⏻</span>
      <span class="reboot-label">재부팅</span>
    </button>
  </div>
</header>

<div id="header-drawer" role="region" aria-label="장치 상태 상세"
     hidden>
  <button class="drawer-close" aria-label="상세 닫기">✕</button>
  <section class="drawer-panel" aria-label="네트워크">
    <h3 class="panel-eyebrow">Network</h3>
    <!-- mode / ssid / ip / rssi rows -->
  </section>
  <section class="drawer-panel" aria-label="트래픽">
    <h3 class="panel-eyebrow">Traffic</h3>
    <!-- RX / TX / TCP clients / queue rows -->
  </section>
  <section class="drawer-panel" aria-label="디바이스">
    <h3 class="panel-eyebrow">Device</h3>
    <!-- uptime / heap / firmware rows -->
  </section>
  <div class="drawer-alerts" role="status" aria-live="polite">
    <h3 class="panel-eyebrow">알림 (<span id="alertTotal">0</span>)</h3>
    <ul id="alertList"><!-- dynamic --></ul>
  </div>
</div>

<dialog id="rebootDialog" aria-modal="true"
        aria-labelledby="rebootTitle" aria-describedby="rebootDesc">
  <h2 id="rebootTitle">장치를 재부팅하시겠어요?</h2>
  <p id="rebootDesc">재부팅 중 약 8초간 연결이 끊깁니다.</p>
  <div class="dialog-actions">
    <button id="rebootCancel" type="button">취소</button>
    <button id="rebootConfirm" type="button" class="destructive">재부팅</button>
  </div>
  <div id="rebootProgress" hidden>
    <span class="spinner" aria-hidden="true">⟳</span>
    <span id="rebootProgressLabel">재부팅 중…</span>
  </div>
</dialog>
```

`<dialog>` element is used directly (native modal semantics, focus trap, Esc handling) — universally supported in ESP-connected browsers (modern Chrome / Safari / Firefox).

## 4. Layout & Interaction Model

### 4.1 Geometry

| Region | Height | Behavior |
|---|---|---|
| Topbar | 46px | Fixed (matches existing `--nav-h`) |
| Drawer (desktop) | ~92px when open (1-alert), grows per alert (max ~200px) | `max-height` transition |
| Drawer (mobile ≤640px) | ~280px when open (stacked) | Same animation |
| Reboot dialog | Centered, max-width 420px, native `::backdrop` | `<dialog.showModal()>` |

### 4.2 Interaction rules

| Event | Result |
|---|---|
| Click `.summary` (anywhere except `#rebootBtn`) | Toggle drawer; `aria-expanded` flips; sessionStorage saves |
| Click `#rebootBtn` | Open `<dialog>` via `showModal()`; drawer state unchanged |
| Click `.drawer-close` | Close drawer; sessionStorage saves |
| Click outside drawer (when open) | Close drawer |
| `Esc` (drawer open, no dialog) | Close drawer |
| `Esc` (dialog open) | Native dialog closes |
| `Enter`/`Space` on `.summary` | Same as click |
| `Tab` order | brand → nav-tabs → `.summary` → drawer (when expanded) → `.drawer-close` → in-panel content |
| Route navigation | Drawer state persists (`sessionStorage`) |
| Page reload | Drawer defaults closed |

### 4.3 Reboot flow

```
[user clicks #rebootBtn]
    ▼
dialog.showModal()                                  ◀── focus traps to dialog
    ▼
[user clicks #rebootConfirm]
    ▼
disable both buttons; show #rebootProgress
    ▼
fetch POST /api/reboot                              ◀── credentials inherited (Basic Auth)
    ├── 200 OK { queued: true, scheduledMs: 500 }
    │       ▼
    │   start polling loop after 2000ms warmup
    │       ▼
    │   setInterval(1000):
    │       fetch GET /api/status
    │           ├── 200 → clearInterval; location.reload()
    │           ├── 401 → keep polling (auth re-prompt is fine on reload)
    │           ├── 503/timeout/network error → keep polling
    │           └── 90s elapsed → stop; show "장치가 응답하지 않습니다" + [닫기][다시 시도]
    │
    ├── 409 (reboot in progress)
    │       ▼
    │   directly enter polling loop (same as 200)
    │
    └── other 4xx/5xx
            ▼
        show inline error; re-enable buttons
```

### 4.4 Drawer panel layout (desktop)

3-column grid via `display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 32px;`. On mobile collapses to `grid-template-columns: 1fr; gap: 16px;`.

Below the three panels, a full-width `.drawer-alerts` section. Single border above (`--panel-divider`).

## 5. Health & Alerts

### 5.1 Health computation

Worst-wins priority: `🔴 fail > 🟡 warn > 🟢 ok > ⊘ offline`.

```
function computeHealth(state, activeAlerts) {
  if (!state) return 'offline';                        // WebSocket and /api/status both unreachable
  if (activeAlerts.some(a => a.severity === 'critical')) return 'fail';
  if (activeAlerts.some(a => a.severity === 'warning')) return 'warn';
  return 'ok';
}
```

| Level | Color token | Glyph | Label |
|---|---|---|---|
| ok | `--status-ok` (cyan) | `●` | `정상` |
| warn | `--status-warn` (amber #f59e0b) | `⚠` | `경고` |
| fail | `--status-fail` (red) | `✕` | `장애` |
| offline | `--status-offline` (gray) | `⊘` | `오프라인` |

### 5.2 Alert sources

| # | Source | Trigger condition | Severity | Lifecycle | Label |
|---|---|---|---|---|---|
| 1 | Wi-Fi connection lost | STA mode `WiFi.status() != WL_CONNECTED` ≥ 5s | critical | level | `Wi-Fi 끊김` |
| 2 | AP provisioning mode | `WiFi.getMode() == WIFI_AP` (sustained) | warning | level | `AP 프로비저닝 모드` |
| 3 | RS485 RX stale (short) | `now - lastRawByteMs > 60s` | warning | level | `RS485 RX 정적 60s+` |
| 4 | RS485 RX stale (long) | `now - lastRawByteMs > 300s` | critical | level | `RS485 RX 정적 5분+` |
| 5 | UART overflow | `uartOverflow` increased in last 60s | warning | edge (TTL 60s) | `UART 오버플로우 N건` |
| 6 | Queue overflow (burst) | `queueOverflow` increased in last 60s | warning | edge (TTL 60s) | `큐 오버플로우 N건` |
| 7 | Queue overflow (sustained) | `queueOverflow` increased in 3 consecutive 60s windows | critical | edge (TTL 300s) | `큐 오버플로우 지속` |
| 8 | TCP client rejected | `tcpRejected` increased in last 60s | warning | edge (TTL 60s) | `TCP 클라이언트 거부 N건` |
| 9 | Queue usage high | `queueUsage / capacity ≥ 80%` | warning | level | `큐 사용률 N%` |
| 10 | Memory low | `ESP.getFreeHeap() < 20480` (20 KB) | critical | level | `메모리 N KB` |
| 11 | Auth failures recent | `authFailures` delta ≥ 3 in last 60s | warning | edge (TTL 60s) | `인증 실패 N건` |
| 12 | Reboot in progress | Local UI state during modal countdown | warning | level (UI-managed) | `재부팅 중…` |

### 5.3 Alert lifecycle implementation

```javascript
// Each alert has a stable id (source kind). Only one entry per id.
// `level` alerts: re-evaluated every state update. Removed when condition false.
// `edge` alerts: created on first observation; ttlMs starts; refreshed on re-observation.

const alerts = new Map();  // id → {severity, label, since, ttlExpiresAt?}

function reconcileAlerts(state) {
  // Level alerts: add or remove based on current condition.
  // Edge alerts: add or refresh based on counter deltas; remove when TTL passed.
  // …
}
```

Display order in drawer: severity desc, then `since` asc (oldest first within severity).

## 6. Drawer Data Binding

### 6.1 Field source map

| Panel | Field | Display | Source | Format |
|---|---|---|---|---|
| Network | mode | "AP" / "STA" | `state.wifi.mode` | text |
| Network | ssid | "dm-d5102q-…" | `state.wifi.ssid` | 24-char ellipsis |
| Network | ip | "192.168.4.18" | `state.wifi.ip` | dotted quad |
| Network | rssi | "-54 dBm" | `state.wifi.rssi` | `${n} dBm` (AP: `—`) |
| Traffic | RX rate | "▲ 12 pkt/s · 1.2 KB/s" | derived from `metrics.rxPackets/rxBytes` | client `deriveRates()` |
| Traffic | TX rate | "▼ 8 pkt/s · 0.8 KB/s" | derived from `metrics.txPackets/txBytes` | client `deriveRates()` |
| Traffic | TCP clients | "⇄ 3/4 클라이언트" | `metrics.tcpClients` + `info.tcp.maxClients` | int/int |
| Traffic | Queue usage | "▮▮▯▯▯ 12/256 (5%)" | `metrics.queueUsage` + `info.queue.capacity` | mini-bar + n/n + pct |
| Device | Uptime | "1h 23m 45s" / "1d 4h 12m" | `metrics.uptime_sec` | `formatUptime()` |
| Device | Heap | "180 KB free" | `metrics.freeHeap / 1024` | int KB |
| Device | Firmware | "v0.1.10 · abc1234" | `info.firmware.version` + `info.firmware.commit` | `v{ver} · {sha7}` |

### 6.2 Rate derivation algorithm

```javascript
let prev = null;

function onStatusUpdate(state) {
  const t = state.metrics.uptime_sec;
  if (prev && t > prev.t) {
    const dt = t - prev.t;
    const rates = deriveRates(prev, state.metrics, dt);
    renderTraffic(rates, state);
  } else {
    renderTraffic({rxPktRate:0, rxByteRate:0, txPktRate:0, txByteRate:0}, state);
  }
  prev = {
    t,
    rxPackets: state.metrics.rxPackets,
    rxBytes:   state.metrics.rxBytes,
    txPackets: state.metrics.txPackets,
    txBytes:   state.metrics.txBytes,
  };
}

function deriveRates(prev, curr, dtSec) {
  if (dtSec <= 0) return {rxPktRate:0, rxByteRate:0, txPktRate:0, txByteRate:0};
  return {
    rxPktRate:  Math.round(wrapDelta(curr.rxPackets, prev.rxPackets) / dtSec),
    rxByteRate: Math.round(wrapDelta(curr.rxBytes,   prev.rxBytes)   / dtSec),
    txPktRate:  Math.round(wrapDelta(curr.txPackets, prev.txPackets) / dtSec),
    txByteRate: Math.round(wrapDelta(curr.txBytes,   prev.txBytes)   / dtSec),
  };
}

function wrapDelta(curr, prev, max = 0xFFFFFFFF) {
  return curr >= prev ? (curr - prev) : ((max - prev) + curr + 1);
}
```

`uptime_sec` is the clock source — eliminates UI clock drift and handles WS reconnection deltas correctly.

### 6.3 Edge cases

| Case | Display |
|---|---|
| `/ws/console` disconnected AND `/api/status` 503/timeout | Summary `⊘ 오프라인`, drawer alerts: `🔴 장치와 통신 불가` |
| Single field missing (e.g. `wifi.rssi` undefined) | Cell shows `—` |
| Provisioning mode (`body.provisioning-only` class set) | Nav-tabs hidden, `#rebootBtn` hidden, summary `⚠ AP 프로비저닝`, drawer Network panel shows AP info, Traffic and Device panels render normally |
| Reboot countdown active | Summary `⟳ 재부팅 중…` (gray), alert list shows `재부팅 중 · 12초 남음` |
| `/api/info` fetch failed | Firmware row `v?.?.?`, queue capacity `—` |

## 7. Visual Style

### 7.1 Icons (unicode-only policy)

| Use | Glyph | Codepoint |
|---|---|---|
| Health: ok / warn / fail / offline | `●` `⚠` `✕` `⊘` | U+25CF / U+26A0 / U+2715 / U+2298 |
| Alert badge | `⚠` | U+26A0 |
| Reboot button | `⏻` | U+23FB |
| Reboot spinner | `⟳` | U+27F3 |
| Traffic RX/TX | `▲` `▼` | U+25B2 / U+25BC |
| TCP clients | `⇄` | U+21C4 |
| Queue mini-bar | `▮` `▯` | U+25AE / U+25AF |
| Network mode AP/STA | `◉` `○` | U+25C9 / U+25CB |
| Drawer toggle hint | `▾` `▴` | U+25BE / U+25B4 |
| Drawer close | `✕` | U+2715 |

Emoji (any `U+1F300-1F9FF` / `U+2700-27BF` outside the whitelist) is disallowed and asserted in test.

### 7.2 Color tokens

```css
:root {
  /* Existing: --rx, --tx, --err, --sys, --nav-h preserved */

  /* New status tokens */
  --status-ok:         var(--sys);
  --status-ok-bg:      rgba(34, 211, 238, 0.10);
  --status-warn:       #f59e0b;
  --status-warn-bg:    rgba(245, 158, 11, 0.12);
  --status-fail:       var(--err);
  --status-fail-bg:    rgba(239, 68, 68, 0.12);
  --status-offline:    #6b7280;
  --status-offline-bg: rgba(107, 114, 128, 0.10);

  /* Drawer surfaces */
  --drawer-bg:         rgba(15, 23, 42, 0.92);
  --panel-divider:     rgba(148, 163, 184, 0.15);
  --panel-label:       rgba(148, 163, 184, 0.70);

  /* Reboot button */
  --reboot-fg:           rgba(248, 250, 252, 0.65);
  --reboot-fg-hover:     var(--err);
  --reboot-border:       rgba(148, 163, 184, 0.25);
  --reboot-border-hover: var(--err);
}
```

Exact existing `:root` values are confirmed against `shared/data/index.html` during M6 implementation; tokens above use the same scheme.

### 7.3 Typography scale

| Element | Family | Size / Weight | Notes |
|---|---|---|---|
| Brand text | system-sans | 15px / 600 | letter-spacing 0.5px |
| Nav-tab | system-sans | 14px / 500 | active: 600 |
| Health label | system-sans | 13px / 500 | |
| Alert badge count | monospace | 13px / 600 | |
| Reboot label | system-sans | 13px / 500 | hidden on mobile |
| Panel eyebrow | system-sans | 11px / 600 / UPPERCASE | letter-spacing 0.8px |
| Panel value (text) | system-sans | 13px / 400 | |
| Panel value (numeric) | monospace | 13px / 400 | `ui-monospace, SFMono-Regular, monospace` |
| Alert message | system-sans | 13px / 500 | |
| Alert timestamp | system-sans | 11px / 400 | dimmed |

### 7.4 Spacing grid (8px base)

- Topbar padding-x: 16px desktop / 12px mobile
- Topbar inter-section gap: 24px
- Summary internal gap: 8px (between health pill, alert badge, reboot)
- Reboot button leading divider: 1px `--panel-divider`, 8px margins each side
- Drawer padding: 24px / 16px (desktop) → 16px / 12px (mobile)
- Drawer 3-col gap: 32px (desktop), 0 stacked (mobile)
- Alerts area top border + margin: 1px + 16px
- Minimum hit target: 32×32px (Summary, Reboot, ✕, alert rows)

### 7.5 Motion

| Element | Duration / Easing | Property |
|---|---|---|
| Drawer expand/collapse | 220ms `cubic-bezier(0.16, 1, 0.3, 1)` | max-height + opacity |
| Health pill color shift | 200ms ease | bg + fg + border |
| Reboot hover | 120ms ease | border + color |
| Alert add/remove | 200ms ease | opacity + translateY(-4px → 0) |
| Reboot spinner | 1.4s linear infinite | rotate 360° |
| Modal backdrop | 150ms ease | opacity |

`prefers-reduced-motion: reduce` overrides all of the above to `0.01ms` or opacity-only.

### 7.6 Accessibility

- Color independence: every state uses color + glyph + text label simultaneously.
- ARIA:
  - Summary: `role="button" aria-expanded aria-controls="header-drawer" aria-label="장치 상태 요약…"`.
  - Drawer: `role="region"`; each panel `<section aria-label="…">`; alerts container `role="status" aria-live="polite"`.
  - Reboot dialog: native `<dialog>` with `aria-modal aria-labelledby aria-describedby`; focus trap via the native element.
- Keyboard: Tab order brand → nav → summary → (drawer focusable children when open) → ✕ → in-panel.
  - Summary: Enter/Space toggles. Esc on body closes drawer when open and no dialog.
- Minimum contrast: 4.5:1 text, 3:1 UI elements. Validated for amber/red on slate-900 in M6.
- Screen reader: alert list announce via `aria-live="polite"` on insertion.

## 8. Backend API Contract

### 8.1 `GET /api/info` (auth required)

Static metadata; client fetches once on header mount and caches.

```jsonc
{
  "firmware": {
    "version": "0.1.10",
    "commit":  "abc1234",
    "buildAt": "2026-05-29T10:00:00Z",
    "board":   "atom-lite"
  },
  "tcp":   { "maxClients": 4 },
  "queue": { "capacity": 256 }
}
```

- `version` derived from `package.json` (and matched in `platformio.ini`).
- `commit` / `buildAt` injected at build time via PlatformIO `extra_scripts`:
  ```python
  Import("env")
  import subprocess, datetime
  commit = subprocess.check_output(["git","rev-parse","--short","HEAD"]).decode().strip()
  built  = datetime.datetime.utcnow().isoformat() + "Z"
  env.Append(BUILD_FLAGS=[f"-DBUILD_COMMIT=\\\"{commit}\\\"", f"-DBUILD_AT=\\\"{built}\\\""])
  ```
- `board` is a compile-time constant per firmware target.

If `/api/info` already exists with other fields, this is a **deep merge** — never drop existing keys.

### 8.2 `GET /api/status` (auth required) — extended

Existing schema preserved. `metrics` object adds:

```jsonc
{
  "metrics": {
    /* existing: uptime_sec, rxPackets, txPackets, rxBytes, txBytes, rawBytes,
       uartOverflow, queueOverflow, droppedPackets, tcpRejected,
       queueUsage, tcpClients, lastRxMs, lastRawByteMs, lastTxMs */

    "freeHeap":       184320,    // NEW: ESP.getFreeHeap() at serialize time, bytes
    "authFailures":   0,         // NEW: cumulative since boot
    "lastAuthFailMs": 0          // NEW: millis() of last 401, 0 if never
  }
}
```

WebSocket `/ws/console` publishes the same schema at 1Hz. Client falls back to polling `/api/status` at 1Hz when WS is not connected.

### 8.3 `POST /api/reboot` (auth required) — NEW

**Request**

```http
POST /api/reboot HTTP/1.1
Authorization: Basic <base64>
Content-Type: application/json

{}
```

Body is currently ignored. JSON envelope reserved for future `{"delaySeconds": …}`.

**Responses**

| Status | Body | Condition |
|---|---|---|
| 200 OK | `{"queued":true,"scheduledMs":500}` | Reboot scheduled |
| 400 | `{"error":"invalid_body"}` | Body parse failure (reserved) |
| 401 | (Basic challenge) | Auth missing/mismatch |
| 409 | `{"error":"reboot_in_progress"}` | `rebootScheduledMs_ != 0` |

**Implementation pattern**

```cpp
// web_server.cpp
server.on("/api/reboot", HTTP_POST, [this](AsyncWebServerRequest* req) {
  if (rebootScheduledMs_ != 0) {
    req->send(409, "application/json",
              R"({"error":"reboot_in_progress"})");
    return;
  }
  rebootScheduledMs_ = millis() + kRebootDelayMs;  // kRebootDelayMs = 500
  req->send(200, "application/json",
            R"({"queued":true,"scheduledMs":500})");
});

// FirmwareApp::poll() — already running every loop tick
void WebServer::pollRebootDeadline() {
  if (rebootScheduledMs_ != 0 && millis() >= rebootScheduledMs_) {
    ESP.restart();  // no return
  }
}
```

Calling `ESP.restart()` inside the request handler would kill the response before flush. The deferred deadline pattern guarantees the 200 reaches the client.

### 8.4 Auth failure tracking

`AsyncAuthenticationMiddleware` from ESPAsyncWebServer is the gate. Failure callback wiring is implementation-dependent:

- **If the library exposes a failure callback API** (e.g. `setAuthFailureCallback(fn)`) — register directly and invoke `status_->recordAuthFailure()`.
- **If not** — define a thin derived class `AuthMiddlewareWithCallback : public AsyncAuthenticationMiddleware` that wraps `run()`, observes the 401 emission, and calls back. Attach the derived instance to the server in `WebServer::begin()`.

This determination is made in the M1 implementation kickoff (5-minute lib spike); the public API in the spec is the same either way.

### 8.5 Security analysis

| Concern | Mitigation |
|---|---|
| Reboot abuse | Global Basic Auth (0.1.9); single-operator surface; no rate-limit needed |
| CSRF | Basic Auth uses no cookie state — cross-site form posts cannot reuse credentials |
| Replay | Reboot is idempotent; subsequent 200 → 409 chain is observable in logs |
| Audit trail | `LogServer` emits `INFO reboot scheduled via API ip=… at=…` on successful 200 |

### 8.6 Backwards compatibility

- `/api/status` additions are pure — no field is renamed or removed.
- `/api/reboot` is new; older UIs simply never call it.
- `/api/info` is deep-merge — existing keys untouched.

## 9. Test Strategy

### 9.1 Existing tests updated

| File | Change |
|---|---|
| `tests/web_server_auth.test.mjs` | Add `{method:'POST', path:'/api/reboot'}` to protected route list; assert handler registration appears after global middleware attach |
| `tests/firmware_structure.test.mjs` | Add required symbols `DeviceStatus::recordAuthFailure`, `WebServer::handleReboot` (or equivalent), `rebootScheduledMs_` to 3-way mirror check |

### 9.2 New test files

| File | Purpose |
|---|---|
| `tests/web_ui_header.test.mjs` | HTML structure: topbar regions, summary `role=button` + `aria-expanded` + `aria-controls`, `#rebootBtn` `aria-haspopup`, `#header-drawer` `role=region`, alerts `role=status aria-live=polite`, `<dialog id="rebootDialog">`, three panel eyebrows (`Network`/`Traffic`/`Device`) |
| `tests/web_ui_no_emoji.test.mjs` | Scan `shared/data/index.html` for any code point in emoji ranges (`U+1F300-1F9FF`, `U+2700-27BF`); whitelist exempts the policy glyph set; assert 0 violations |
| `tests/api_contract.test.mjs` | Regex assertions in `shared/src/web/web_server.cpp`: `/api/info` handler present + payload mentions `version`, `tcp`, `queue`; `/api/reboot` handler present + literals `"queued":true`, `"reboot_in_progress"`; `FirmwareApp::poll` references `pollRebootDeadline` or `rebootScheduledMs_` |
| `tests/status_metrics_contract.test.mjs` | Header asserts new `uint32_t authFailures`, `lastAuthFailMs` fields and `recordAuthFailure()` declaration; status JSON serializer source contains literal `"freeHeap"`, `"authFailures"`, `"lastAuthFailMs"` |
| `tests/header_logic.test.mjs` | Pure-function unit tests for `deriveRates`, `wrapDelta`, `computeHealth`, `formatUptime`. Extracts inline `<script>` via regex + executes in `vm` context. Requires the script to export `window.__headerExports = {deriveRates, wrapDelta, computeHealth, formatUptime}` (or equivalent attachment) so tests can read them |

### 9.3 TDD work order

For each milestone (Section 10), the loop is:

1. **RED** — add failing assertion to the test file
2. **GREEN** — minimum code change to pass
3. **REFACTOR** — tighten, dedupe; tests stay green

Test files exist at the start of each milestone; assertions are added as features land.

### 9.4 Client-side function extraction

`shared/data/index.html` retains the single inline `<script>` block. At the end of the script:

```javascript
// Test export hook — present in production too (harmless ~80 bytes)
window.__headerExports = {
  deriveRates, wrapDelta, computeHealth, formatUptime
};
```

Tests use `vm.runInContext` with a minimal stub `{window: {}, document: {}}` ctx, then read `ctx.window.__headerExports`. No build-time JS extraction required.

### 9.5 Manual E2E checklist

Run on both atom-lite and atoms3-lite hardware before release.

| # | Scenario | Expected |
|---|---|---|
| 1 | Normal boot, header loads | Summary `● 정상`, drawer closed |
| 2 | Click summary | Drawer expands (220ms), 3 panels populated |
| 3 | Route navigation (dashboard → console) | Drawer stays open |
| 4 | Page reload | Drawer closed (default) |
| 5 | Press Esc | Drawer closes |
| 6 | Click outside drawer | Drawer closes |
| 7 | Click reboot button | Confirm dialog appears |
| 8 | Confirm reboot → device restarts | Dialog → "재부팅 중…" → auto-reload within 90s |
| 9 | Cancel reboot dialog | Dialog closes; header unchanged |
| 10 | Force Wi-Fi loss (block at router) | Summary turns `✕ 장애 ⚠1`, "Wi-Fi 끊김" in alerts |
| 11 | Restore Wi-Fi | Summary returns to `● 정상` within 5s |
| 12 | Halt RS485 RX 60s+ | Summary `⚠ 경고 ⚠1`, "RS485 RX 정적 60s+" |
| 13 | Trigger 3 auth failures | Summary `⚠ 경고 ⚠1`, "인증 실패 3건" |
| 14 | Mobile viewport (≤640px, devtools) | Nav becomes hamburger, drawer stacks, reboot shows ⏻ only |
| 15 | `prefers-reduced-motion: reduce` | All transitions disabled |
| 16 | Screen reader (VoiceOver/NVDA) | Drawer toggle announces expanded/collapsed; new alert announced |
| 17 | Keyboard only (Tab/Enter/Esc) | Every action reachable |
| 18 | AP provisioning mode (no Wi-Fi configured) | Nav and reboot hidden, summary `⚠ AP 프로비저닝` |
| 19 | atoms3-lite build | Same behavior as atom-lite, board field differs |
| 20 | Post-OTA first load | `/api/info` commit SHA reflects new build |

CI runs node tests only; manual checklist runs before tag/release.

## 10. Milestones & Definition of Done

### 10.1 Milestone breakdown

Each milestone is one atomic commit. Test-first within the milestone.

| M# | Title | Output | Depends |
|---|---|---|---|
| M1 | DeviceStatus model extension | `authFailures`, `lastAuthFailMs`, `recordAuthFailure()`, JSON serializer adds `freeHeap` | — |
| M2 | `/api/info` + `/api/status` extension | WebServer routes, payloads, build-time meta injection | M1 |
| M3 | `/api/reboot` + reboot deadline poll | WebServer handler, `WebServer::pollRebootDeadline`, FirmwareApp wiring, 409 idempotency | M1 |
| M4 | 3-way mirror sync | M1–M3 changes replicated into `atom-lite/` and `atoms3-lite/` trees | M1, M2, M3 |
| M5 | HTML markup (no logic) | Topbar restructure, drawer container, `<dialog>` modal, ARIA, glyph set | — |
| M6 | CSS styles | Status tokens, drawer panels, transitions, `prefers-reduced-motion` | M5 |
| M7 | JS pure functions | `deriveRates`, `computeHealth`, `formatUptime`, `wrapDelta`, test export hook | M5 |
| M8 | JS UI wiring | Drawer toggle, summary updates, modal flow, reboot polling, alert renderer | M6, M7 |
| M9 | Mobile responsive | Hamburger nav, reboot label toggle, drawer 1-column stack | M6, M8 |
| M10 | Documentation + version bump | `CHANGELOG`, `CHANGELOG.en`, `README` (link new endpoints), `docs/features/dashboard.md`, `docs/api/http.md`, `docs/api/http.en.md`, `package.json`, `platformio.ini`, version 0.1.10 sync | M1–M9 |
| M11 | Manual E2E + build size | Checklist run on both boards, firmware size delta check, smoke test recorded | M10 |

Estimated effort: M1–M4 (~6h backend), M5–M9 (~10h frontend, M8 largest), M10–M11 (~2h wrap).

### 10.2 Definition of Done

- [ ] All new/updated tests pass (`node --test tests/*.test.mjs`)
- [ ] 3-way mirror synchronized; `firmware_structure.test.mjs` green
- [ ] Zero emoji in `shared/data/index.html`; `web_ui_no_emoji.test.mjs` green
- [ ] WCAG 2.1 AA contrast verified for amber/red/cyan on slate-900
- [ ] Both boards (atom-lite, atoms3-lite) build cleanly
- [ ] Firmware binary size growth < 5 KB on each board
- [ ] Manual E2E checklist ≥18/20 pass (exceptions documented)
- [ ] `CHANGELOG.md` and `CHANGELOG.en.md` 0.1.10 sections added (Keep-a-Changelog format)
- [ ] `docs/api/http.md` and `docs/api/http.en.md` document `/api/info` and `/api/reboot`
- [ ] `docs/features/dashboard.md` and `.en.md` describe the header drawer behavior
- [ ] `README.md` Quick Start references the reboot button
- [ ] `web_server_auth.test.mjs` confirms `/api/reboot` lives behind global middleware
- [ ] One VoiceOver or NVDA smoke pass recorded for drawer toggle, alert announce, reboot dialog

## 11. Risks & Open Questions

| Risk / Assumption | Impact | Response |
|---|---|---|
| `AsyncAuthenticationMiddleware` may not expose a failure callback | M1 auth tracking blocked | Derived class wrap path noted in §8.4; decided during M1 first hour |
| `/ws/console` 1Hz cadence assumed | Rate resolution = 1s | Acceptable; if not, follow-up PR tunes cadence |
| STA-mode DHCP rebind on reboot | Polling never reaches new IP | 90s timeout falls back to manual UI; mDNS exploration deferred |
| Firmware size +5 KB ceiling | Tight flash margin | Measured in M11; if exceeded, drop optional glyphs first |
| `prefers-reduced-motion` may not catch all environments | A few users see motion | Manual E2E #15 verifies; supplement with `prefers-reduced-data` if needed |

## 12. Out of Scope

The following are deliberately **not** in this PR; tracked as future follow-ups:

- Light theme variant
- OTA progress indicator in header (transient alert allowed; persistent panel later)
- Multi-device profile switcher in header
- NTP-synced wall-clock display
- Build-time git diff link from header
- Redesign of console / scanner / settings / OTA / info sub-pages
- Server-pushed per-second rates (current spec uses client-derived rates)
- Rate limiting on `/api/reboot` (single-operator assumption)
