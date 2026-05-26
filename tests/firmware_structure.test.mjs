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
  "shared/src/status/status_led.h",
  "shared/src/status/status_led.cpp",
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
  "shared/src/web/admin_ui.h",
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
  "class StatusLed",
  "class ConfigStore",
  "class WebServer",
  "class OtaManager",
  "parseHex",
  "bytesToHex",
  "/api/status",
  "/api/config",
  "/api/wifi/scan",
  "/api/tx",
  "/api/scanner/start",
  "/api/scanner/stop",
  "/api/factory-reset",
  "/ws/console",
  "/update",
  "kAdminIndexHtmlGz",
  "kAdminIndexHtmlGzLen",
  "beginResponse_P",
  "Content-Encoding",
  "gzip",
  "0.1.2",
  "uint32_t baud = 3840",
  "Preferences",
  "Update",
  "WiFiServer",
  "WiFi.scanNetworks",
  "AsyncWebSocket",
  "HardwareSerial",
  "Adafruit_NeoPixel",
  "STATUS_LED_COUNT",
  "statusLed_",
]) {
  assert.match(allSource, new RegExp(symbol.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")), `missing symbol ${symbol}`);
}

for (const board of ["atoms3-lite", "atom-lite"]) {
  assert.ok(existsSync(join(root, `firmware/${board}/src/app/firmware_app.cpp`)), `missing synced shared src for ${board}`);
  assert.ok(existsSync(join(root, `firmware/${board}/data/index.html`)), `missing synced index.html for ${board}`);

  assert.ok(existsSync(join(root, `firmware/${board}/src/web/admin_ui.h`)), `missing synced embedded admin UI for ${board}`);
}

const webServer = readFileSync(join(root, "shared/src/web/web_server.cpp"), "utf8");
const otaManager = readFileSync(join(root, "shared/src/web/ota_manager.cpp"), "utf8");
const rs485Driver = readFileSync(join(root, "shared/src/rs485/rs485_driver.cpp"), "utf8");
const atomLiteBoard = readFileSync(join(root, "firmware/atom-lite/board_config.h"), "utf8");
assert.doesNotMatch(webServer, /LittleFS\.begin|serveStatic/, "web UI must be embedded in firmware, not served from LittleFS");
assert.doesNotMatch(otaManager, /U_LITTLEFS|LittleFS/, "OTA must flash a single firmware image only");
assert.match(rs485Driver, /RS485_DE_PIN\s*>=\s*0/, "RS485 DE pin must be optional for Tail485");
assert.match(atomLiteBoard, /#define RS485_RX_PIN 32/, "Atom Lite Tail485 RX must use GPIO32");
assert.match(atomLiteBoard, /#define RS485_TX_PIN 26/, "Atom Lite Tail485 TX must use GPIO26");
assert.match(atomLiteBoard, /#define RS485_DE_PIN -1/, "Tail485 has no discrete DE pin");
