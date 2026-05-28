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
  // ArduinoJson assignment syntax — match actual C++ source, not JSON literals
  assert.match(ws, /\["firmware"\]/);
  assert.match(ws, /fw\["version"\]\s*=\s*"0\.1\.10"/);
  assert.match(ws, /tcp\["max_clients"\]\s*=/);
  assert.match(ws, /queue\["capacity"\]\s*=/);
});
