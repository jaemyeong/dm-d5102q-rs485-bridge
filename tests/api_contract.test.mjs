import assert from "node:assert/strict";
import { test } from "node:test";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const wsh = readFileSync(join(root, "shared/src/web/web_server.h"), "utf8");
const ws  = readFileSync(join(root, "shared/src/web/web_server.cpp"), "utf8");

test("WebServer registers GET /api/info", () => {
  assert.match(ws, /server_?\.\s*on\s*\(\s*"\/api\/info"\s*,\s*HTTP_GET/);
});

test("WebServer declares handleInfo", () => {
  assert.match(wsh, /void\s+handleInfo\s*\(\s*AsyncWebServerRequest/);
});

test("/api/info payload exposes firmware/tcp/queue blocks with required fields", () => {
  // Match ArduinoJson C++ assignment syntax, scoped to the right parent object
  assert.match(ws, /data\["firmware"\]\.to<JsonObject>/);
  assert.match(ws, /fw\["version"\]\s*=\s*"0\.1\.10"/);
  assert.match(ws, /fw\["commit"\]\s*=\s*BUILD_COMMIT/);
  assert.match(ws, /fw\["built_at"\]\s*=\s*BUILD_AT/);
  assert.match(ws, /fw\["board"\]\s*=\s*ARDUINO_BOARD/);
  assert.match(ws, /tcp\["max_clients"\]\s*=/);
  assert.match(ws, /queue\["capacity"\]\s*=/);
});

test("WebServer registers POST /api/reboot", () => {
  assert.match(ws, /server_?\.\s*on\s*\(\s*"\/api\/reboot"\s*,\s*HTTP_POST/);
});

test("WebServer declares handleReboot and rebootScheduledMs_", () => {
  assert.match(wsh, /void\s+handleReboot\s*\(/);
  assert.match(wsh, /uint32_t\s+rebootScheduledMs_/);
});

test("/api/reboot 200 path returns queued:true and scheduledMs", () => {
  assert.match(ws, /"queued"\s*:\s*true/);
  assert.match(ws, /"scheduledMs"\s*:\s*500/);
});
