import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const html = readFileSync(join(root, "shared/data/index.html"), "utf8");

assert.match(html, /<title>DM-D5102Q RS485 TCP Bridge<\/title>/);
assert.doesNotMatch(html, /https?:\/\//);
assert.doesNotMatch(html, /type="text\/babel"/);
assert.doesNotMatch(html, /react/i);
assert.match(html, /id="app"/);

for (const route of [
  "dashboard",
  "console",
  "scanner",
  "settings",
  "provisioning",
  "ota",
  "about",
]) {
  assert.match(html, new RegExp(`data-route="${route}"`));
}

for (const endpoint of [
  "/ws/console",
  "/api/status",
  "/api/config",
  "/api/tx",
  "/api/scanner/start",
  "/api/scanner/stop",
  "/api/factory-reset",
  "/update",
]) {
  assert.match(html, new RegExp(endpoint.replace(/[/.]/g, "\\$&")));
}

for (const requiredText of [
  "Wi-Fi Provisioning",
  "RS485 Console",
  "HEX TX",
  "TCP Bridge",
  "OTA Update",
  "Overflow",
  "Factory Reset",
]) {
  assert.match(html, new RegExp(requiredText));
}

assert.match(html, /MAX_CONSOLE_LINES\s*=\s*300/);
assert.match(html, /MAX_TX_BYTES\s*=\s*256/);
assert.match(html, /CONSOLE_BATCH_MS\s*=\s*50/);
assert.match(html, /function parseHexInput/);
assert.match(html, /function addConsolePacket/);
assert.match(html, /function connectConsoleSocket/);
assert.match(html, /function saveConfig/);
assert.match(html, /function uploadFirmware/);
assert.match(html, /function startScan/);
