import assert from "node:assert/strict";
import { test } from "node:test";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const h = readFileSync(join(root, "shared/src/status/device_status.h"), "utf8");
const c = readFileSync(join(root, "shared/src/status/device_status.cpp"), "utf8");

test("DeviceStatus declares auth_failures and last_auth_fail_ms fields", () => {
  assert.match(h, /uint32_t\s+authFailures\s*=\s*0/);
  assert.match(h, /uint32_t\s+lastAuthFailMs\s*=\s*0/);
});

test("DeviceStatus declares recordAuthFailure()", () => {
  assert.match(h, /void\s+recordAuthFailure\s*\(\s*\)/);
});

test("recordAuthFailure increments counter and updates timestamp", () => {
  assert.match(c, /void\s+DeviceStatus::recordAuthFailure\s*\(\s*\)\s*{[\s\S]*?authFailures\+\+[\s\S]*?lastAuthFailMs\s*=\s*millis\(\)/);
});
