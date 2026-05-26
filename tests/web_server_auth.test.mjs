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
