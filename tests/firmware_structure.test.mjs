import assert from "node:assert/strict";
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = dirname(dirname(fileURLToPath(import.meta.url)));

const requiredFiles = [
  "shared/src/app/firmware_app.h",
  "shared/src/app/firmware_app.cpp",
  "shared/src/app/scheduler.h",
  "shared/src/app/scheduler.cpp",
  "shared/src/network/wifi_manager.h",
  "shared/src/network/wifi_manager.cpp",
  "shared/src/network/tcp_bridge.h",
  "shared/src/network/tcp_bridge.cpp",
  "shared/src/rs485/rs485_driver.h",
  "shared/src/rs485/rs485_driver.cpp",
  "shared/src/rs485/packet_parser.h",
  "shared/src/rs485/packet_parser.cpp",
  "shared/src/rs485/packet_queue.h",
  "shared/src/rs485/packet_queue.cpp",
  "shared/src/rs485/packet.h",
  "shared/src/scanner/baud_scanner.h",
  "shared/src/scanner/baud_scanner.cpp",
  "shared/src/status/device_status.h",
  "shared/src/status/device_status.cpp",
  "shared/src/storage/config_store.h",
  "shared/src/storage/config_store.cpp",
  "shared/src/utils/hex.h",
  "shared/src/utils/hex.cpp",
  "shared/src/utils/time.h",
  "shared/src/utils/time.cpp",
  "shared/src/web/web_server.h",
  "shared/src/web/web_server.cpp",
  "shared/src/web/ota_manager.h",
  "shared/src/web/ota_manager.cpp",
  "firmware/atoms3-lite/atoms3-lite.ino",
  "firmware/atoms3-lite/board_config.h",
  "firmware/atoms3-lite/secrets.h.example",
  "firmware/atoms3-lite/partitions.csv",
  "firmware/atom-lite/atom-lite.ino",
  "firmware/atom-lite/board_config.h",
  "firmware/atom-lite/secrets.h.example",
  "firmware/atom-lite/partitions.csv",
  "tools/sync_shared.sh",
];

for (const file of requiredFiles) {
  assert.ok(existsSync(join(root, file)), `missing ${file}`);
}

const allSource = requiredFiles
  .filter((file) => file.endsWith(".h") || file.endsWith(".cpp") || file.endsWith(".ino"))
  .map((file) => readFileSync(join(root, file), "utf8"))
  .join("\n");

for (const symbol of [
  "class FirmwareApp",
  "class Scheduler",
  "class BridgeWifiManager",
  "class TcpBridge",
  "class Rs485Driver",
  "class PacketParser",
  "class PacketQueue",
  "class BaudScanner",
  "class DeviceStatus",
  "class ConfigStore",
  "class WebServer",
  "class OtaManager",
  "parseHex",
  "bytesToHex",
  "/api/status",
  "/api/config",
  "/api/tx",
  "/api/scanner/start",
  "/api/scanner/stop",
  "/api/factory-reset",
  "/ws/console",
  "/update",
  "Preferences",
  "LittleFS",
  "Update",
  "WiFiServer",
  "AsyncWebSocket",
  "HardwareSerial",
]) {
  assert.match(allSource, new RegExp(symbol.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")), `missing symbol ${symbol}`);
}

for (const board of ["atoms3-lite", "atom-lite"]) {
  assert.ok(existsSync(join(root, `firmware/${board}/src/app/firmware_app.cpp`)), `missing synced shared src for ${board}`);
  assert.ok(existsSync(join(root, `firmware/${board}/data/index.html`)), `missing synced index.html for ${board}`);

  const partitions = readFileSync(join(root, `firmware/${board}/partitions.csv`), "utf8");
  assert.match(partitions, /^littlefs,\s*data,\s*littlefs,/m, `${board} must define a littlefs data partition`);
}

const webServer = readFileSync(join(root, "shared/src/web/web_server.cpp"), "utf8");
assert.match(
  webServer,
  /LittleFS\.begin\(\s*true\s*,\s*"\/littlefs"\s*,\s*10\s*,\s*"littlefs"\s*\)/,
  "LittleFS must mount the explicit littlefs partition label used by partitions.csv",
);
