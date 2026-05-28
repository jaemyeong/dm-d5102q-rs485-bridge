import assert from "node:assert/strict";
import { test } from "node:test";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const html = readFileSync(join(root, "shared/data/index.html"), "utf8");

test("topbar renders summary cluster as ARIA button", () => {
  assert.match(html, /<div[^>]*class="summary"[^>]*role="button"/);
  assert.match(html, /aria-expanded="false"/);
  assert.match(html, /aria-controls="header-drawer"/);
});

test("summary cluster contains health pill, alert count, and reboot button", () => {
  assert.match(html, /id="hdrHealth"[^>]*class="health-pill/);
  assert.match(html, /id="hdrAlertCount"/);
  assert.match(html, /id="hdrReboot"[^>]*aria-haspopup="dialog"/);
});

test("drawer container has role region and live alerts list", () => {
  assert.match(html, /id="header-drawer"[^>]*role="region"/);
  assert.match(html, /<div[^>]*class="drawer-alerts"[^>]*role="status"[^>]*aria-live="polite"/);
});

test("three drawer panels are present with eyebrow headers", () => {
  for (const heading of ["Network", "Traffic", "Device"]) {
    assert.ok(html.includes(`>${heading}<`),
      `panel header "${heading}" not found in admin UI`);
  }
});

test("reboot confirmation dialog uses native <dialog>", () => {
  assert.match(html, /<dialog[^>]*id="rebootDialog"[^>]*aria-modal="true"/);
  assert.match(html, /id="rebootCancel"/);
  assert.match(html, /id="rebootConfirm"/);
  assert.match(html, /id="rebootProgress"/);
});

test("drawer close button exists", () => {
  assert.match(html, /class="drawer-close"[^>]*aria-label/);
});
