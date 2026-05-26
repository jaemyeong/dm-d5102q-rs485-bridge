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
