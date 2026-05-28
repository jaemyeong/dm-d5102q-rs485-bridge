import assert from "node:assert/strict";
import { test } from "node:test";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const html = readFileSync(join(root, "shared/data/index.html"), "utf8");

const ALLOWED = new Set([
  "●", "▲", "▼", "✓", "✕", "⚠", "⏻",
  "⊘", "⟳", "⇄", "▮", "▯", "◉", "○", "▾", "▴"
]);

const EMOJI_RE = /[\u{1F300}-\u{1F9FF}\u{1FA70}-\u{1FAFF}\u{2600}-\u{27BF}]/gu;

test("shared/data/index.html contains no emoji outside the whitelist", () => {
  const violations = [];
  for (const match of html.matchAll(EMOJI_RE)) {
    if (!ALLOWED.has(match[0])) violations.push(match[0]);
  }
  const uniq = [...new Set(violations)];
  assert.equal(
    violations.length,
    0,
    `forbidden glyphs in admin UI: ${uniq.join(", ")} (total ${violations.length} occurrences)`
  );
});
