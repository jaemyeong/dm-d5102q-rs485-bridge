import assert from "node:assert/strict";
import { test } from "node:test";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import vm from "node:vm";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const html = readFileSync(join(root, "shared/data/index.html"), "utf8");

const m = html.match(/<script id="header-pure-fns">([\s\S]*?)<\/script>/);
assert.ok(m, '<script id="header-pure-fns"> not found in admin UI');
const body = m[1];

const ctx = { window: {} };
vm.createContext(ctx);
vm.runInContext(body, ctx);
const fns = ctx.window.__headerExports;
assert.ok(fns, "window.__headerExports not set by header-pure-fns script");
const { wrapDelta, deriveRates, computeHealth, formatUptime } = fns;

test("wrapDelta handles normal delta", () => {
  assert.equal(wrapDelta(120, 100), 20);
});

test("wrapDelta handles uint32 wraparound", () => {
  assert.equal(wrapDelta(5, 0xFFFFFFF0), 0x15);
});

test("deriveRates computes per-second rates", () => {
  const r = deriveRates(
    { rxPackets: 100, rxBytes: 1000, txPackets: 80, txBytes: 800 },
    { rxPackets: 112, rxBytes: 2200, txPackets: 88, txBytes: 1600 },
    1.0
  );
  assert.equal(r.rxPktRate, 12);
  assert.equal(r.rxByteRate, 1200);
  assert.equal(r.txPktRate, 8);
  assert.equal(r.txByteRate, 800);
});

test("deriveRates returns zeros when dt <= 0", () => {
  const r = deriveRates(
    { rxPackets: 100, rxBytes: 1000, txPackets: 80, txBytes: 800 },
    { rxPackets: 200, rxBytes: 2000, txPackets: 160, txBytes: 1600 },
    0
  );
  assert.deepEqual(r, { rxPktRate: 0, rxByteRate: 0, txPktRate: 0, txByteRate: 0 });
});

test("computeHealth offline when state null", () => {
  assert.equal(computeHealth(null, []), "offline");
});

test("computeHealth ok when no alerts", () => {
  assert.equal(computeHealth({}, []), "ok");
});

test("computeHealth warn for warning alerts", () => {
  assert.equal(computeHealth({}, [{ severity: "warning" }]), "warn");
});

test("computeHealth fail when any critical present", () => {
  assert.equal(computeHealth({}, [
    { severity: "warning" }, { severity: "critical" }
  ]), "fail");
});

test("formatUptime under a minute", () => {
  assert.equal(formatUptime(45), "45s");
});

test("formatUptime mins and secs", () => {
  assert.equal(formatUptime(125), "2m 5s");
});

test("formatUptime hours mins secs", () => {
  assert.equal(formatUptime(5000), "1h 23m 20s");
});

test("formatUptime days threshold", () => {
  assert.equal(formatUptime(90061), "1d 1h 1m");
});
