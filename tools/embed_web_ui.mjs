import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gzipSync } from "node:zlib";

const root = dirname(dirname(fileURLToPath(import.meta.url)));
const sourcePath = join(root, "shared/data/index.html");
const targetPath = join(root, "shared/src/web/admin_ui.h");

const html = readFileSync(sourcePath);
const gzipped = gzipSync(html, { level: 9 });

const rows = [];
for (let i = 0; i < gzipped.length; i += 12) {
  rows.push(
    "  " +
      Array.from(gzipped.subarray(i, i + 12))
        .map((byte) => `0x${byte.toString(16).padStart(2, "0")}`)
        .join(", ") +
      (i + 12 < gzipped.length ? "," : "")
  );
}

const output = `#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace dm {

inline constexpr size_t kAdminIndexHtmlGzLen = ${gzipped.length};
inline const uint8_t kAdminIndexHtmlGz[] PROGMEM = {
${rows.join("\n")}
};

}  // namespace dm
`;

writeFileSync(targetPath, output);
