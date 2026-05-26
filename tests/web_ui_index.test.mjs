import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const html = readFileSync(join(root, "shared/data/index.html"), "utf8");

assert.match(html, /<title>DM-D5102Q RS485 TCP 브리지<\/title>/);
assert.doesNotMatch(html, /https?:\/\//);
assert.doesNotMatch(html, /type="text\/babel"/);
assert.doesNotMatch(html, /react/i);
assert.doesNotMatch(html, /name="basic_auth"/);
assert.doesNotMatch(html, /id="cfgBasicAuth"/);
assert.doesNotMatch(html, /basic_auth\s*:/);
assert.match(html, /id="app"/);

for (const route of [
  "dashboard",
  "console",
  "scanner",
  "settings",
  "ota",
  "about",
  "provisioning",
  "rebooting",
]) {
  assert.match(html, new RegExp(`data-route="${route}"`));
}

for (const endpoint of [
  "/ws/console",
  "/api/status",
  "/api/config",
  "/api/wifi/scan",
  "/api/tx",
  "/api/scanner/start",
  "/api/scanner/stop",
  "/api/factory-reset",
  "/update",
]) {
  assert.match(html, new RegExp(endpoint.replace(/[/.]/g, "\\$&")));
}

for (const requiredText of [
  "Wi-Fi 프로비저닝",
  "RS485 콘솔",
  "HEX TX",
  "TCP 브리지",
  "OTA 업데이트",
  "오버플로",
  "공장 초기화",
  "재부팅 중입니다",
  "Wi-Fi 설정 없이 펌웨어 업데이트",
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
assert.match(html, /function scheduleOtaReload/);
assert.match(html, /function startScan/);
assert.match(html, /function loadWifiNetworks/);
assert.match(html, /function normalizeNetworks/);
assert.match(html, /function isProvisioningMode/);
assert.match(html, /function isProvisioningAllowedRoute/);
assert.match(html, /isProvisioningAllowedRoute\(requested\)/);
assert.match(html, /let settingsDirty = false/);
assert.match(html, /function refreshSettingsFromState/);
assert.match(html, /activeRoute === "settings" && !settingsDirty/);
assert.match(html, /function routeFromPath/);
assert.match(html, /history\.pushState/);
assert.match(html, /window\.addEventListener\("popstate"/);
assert.match(html, /body\.provisioning-only/);
assert.match(html, /id="cfgBaud"[^>]+type="number"/);
assert.match(html, /id="cfgBaud"[^>]+value="3840"/);
assert.match(html, /id="baudPresets"/);
assert.match(html, /id="settingsSsidList"/);
assert.match(html, /id="refreshSettingsNetworks"/);
assert.match(html, /function refreshSettingsNetworks/);
assert.match(html, /window\.location\.reload/);
assert.match(html, /if \(!LOCAL_DEMO\) return;\s*const samples/);
assert.match(html, /version: "0\.1\.9"/);
assert.match(html, /baud: 3840/);
assert.doesNotMatch(html, /DIRECT-7C-HP/);
assert.doesNotMatch(html, /<select id="cfgBaud"/);
assert.doesNotMatch(html, /WIFI_SCAN_POLL_MS/);
assert.doesNotMatch(html, /location\.hash/);
assert.doesNotMatch(html, /data-route="provisioning"[^>]*>Provisioning/);
assert.doesNotMatch(html, /bindEvents\(\);\s*renderNetworks\(\[\], "scanning"\);\s*fillSettings\(\);/);
