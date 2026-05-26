#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_NeoPixel.h>
#include <stdarg.h>

constexpr int8_t   kRs485RxPin    = 1;
constexpr int8_t   kRs485TxPin    = 2;
constexpr uint32_t kRs485Baud     = 3840;

constexpr int8_t   kLedPin        = 35;
constexpr uint16_t kLedCount      = 1;
constexpr uint8_t  kLedBrightness = 96;
constexpr uint32_t kBlinkHoldMs   = 120;

constexpr char     kWifiSsid[]    = "Magi";
constexpr char     kWifiPass[]    = "06090319";

constexpr uint16_t kRxPort        = 8899;
constexpr uint16_t kLogPort       = 8898;
constexpr uint8_t  kMaxRxClients  = 3;
constexpr uint8_t  kMaxLogClients = 2;
constexpr uint32_t kHeartbeatMs   = 2000;
constexpr uint32_t kWifiTimeoutMs = 20000;

Adafruit_NeoPixel pixel(kLedCount, kLedPin, NEO_GRB + NEO_KHZ800);
WiFiServer rxServer(kRxPort);
WiFiServer logServer(kLogPort);
WiFiClient rxClients[kMaxRxClients];
WiFiClient logClients[kMaxLogClients];

uint32_t lastRxMs        = 0;
uint64_t totalRxBytes    = 0;
uint64_t totalTxBytes    = 0;
uint32_t lastHeartbeatMs = 0;
bool     ledOn           = false;

uint8_t countConnected(WiFiClient* arr, uint8_t cap) {
  uint8_t n = 0;
  for (uint8_t i = 0; i < cap; i++) {
    if (arr[i] && arr[i].connected()) n++;
  }
  return n;
}

void logf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  int len = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  if (len <= 0) return;
  if (len >= static_cast<int>(sizeof(buf))) len = sizeof(buf) - 1;
  Serial.write(reinterpret_cast<const uint8_t*>(buf), len);
  for (uint8_t i = 0; i < kMaxLogClients; i++) {
    if (logClients[i] && logClients[i].connected()) {
      logClients[i].write(reinterpret_cast<const uint8_t*>(buf), len);
    }
  }
}

void setLed(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);
  WiFi.begin(kWifiSsid, kWifiPass);
  Serial.printf("[rxtest] WiFi connecting to \"%s\" ", kWifiSsid);
  const uint32_t started = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - started < kWifiTimeoutMs) {
    delay(200);
    Serial.print('.');
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[rxtest] WiFi connected, IP=");
    Serial.print(WiFi.localIP());
    Serial.printf(" RSSI=%ddBm\n", WiFi.RSSI());
  } else {
    Serial.println("[rxtest] WiFi connect timeout (autoreconnect ON)");
  }
}

void acceptOnto(WiFiServer& server, WiFiClient* arr, uint8_t cap, const char* tag) {
  WiFiClient incoming = server.available();
  if (!incoming) return;
  for (uint8_t i = 0; i < cap; i++) {
    if (!arr[i] || !arr[i].connected()) {
      arr[i].stop();
      arr[i] = incoming;
      arr[i].setNoDelay(true);
      logf("[rxtest] %s client[%u] connected: %s\n", tag, i,
           arr[i].remoteIP().toString().c_str());
      return;
    }
  }
  logf("[rxtest] %s client rejected: slots full\n", tag);
  incoming.stop();
}

void reapClients(WiFiClient* arr, uint8_t cap, const char* tag) {
  for (uint8_t i = 0; i < cap; i++) {
    if (arr[i] && !arr[i].connected()) {
      logf("[rxtest] %s client[%u] disconnected\n", tag, i);
      arr[i].stop();
    }
  }
}

void broadcastRx(const uint8_t* data, size_t length) {
  for (uint8_t i = 0; i < kMaxRxClients; i++) {
    if (rxClients[i] && rxClients[i].connected()) {
      size_t wrote = rxClients[i].write(data, length);
      totalTxBytes += wrote;
      if (wrote != length) {
        logf("[rxtest] rx client[%u] short write %u/%u\n", i,
             static_cast<unsigned>(wrote), static_cast<unsigned>(length));
      }
    }
  }
}

void heartbeat() {
  uint32_t now = millis();
  if (now - lastHeartbeatMs < kHeartbeatMs) return;
  lastHeartbeatMs = now;
  IPAddress ip = WiFi.localIP();
  int8_t rssi = WiFi.RSSI();
  uint8_t rxN = countConnected(rxClients, kMaxRxClients);
  uint8_t logN = countConnected(logClients, kMaxLogClients);
  uint32_t lastRxAgo = (lastRxMs == 0) ? 0xFFFFFFFFu : (now - lastRxMs);
  bool wifiUp = WiFi.status() == WL_CONNECTED;
  logf("[rxtest] hb up=%lus wifi=%d ip=%s rssi=%d rx=%u log=%u "
       "rxB=%llu txB=%llu lastRxAgoMs=%lu\n",
       static_cast<unsigned long>(now / 1000), wifiUp ? 1 : 0,
       ip.toString().c_str(), static_cast<int>(rssi), rxN, logN,
       static_cast<unsigned long long>(totalRxBytes),
       static_cast<unsigned long long>(totalTxBytes),
       static_cast<unsigned long>(lastRxAgo));
}

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("[rxtest] boot");
  Serial.printf("[rxtest] Serial2 RX=GPIO%d TX=GPIO%d baud=%u 8N1\n",
                kRs485RxPin, kRs485TxPin, kRs485Baud);

  pixel.begin();
  pixel.setBrightness(kLedBrightness);
  setLed(0, 0, 0);

  Serial2.begin(kRs485Baud, SERIAL_8N1, kRs485RxPin, kRs485TxPin);

  connectWifi();
  rxServer.begin();
  rxServer.setNoDelay(true);
  logServer.begin();
  logServer.setNoDelay(true);
  Serial.printf("[rxtest] RX server  on :%u (max %u clients)\n", kRxPort, kMaxRxClients);
  Serial.printf("[rxtest] LOG server on :%u (max %u clients)\n", kLogPort, kMaxLogClients);
}

void loop() {
  acceptOnto(rxServer,  rxClients,  kMaxRxClients,  "rx");
  acceptOnto(logServer, logClients, kMaxLogClients, "log");
  reapClients(rxClients,  kMaxRxClients,  "rx");
  reapClients(logClients, kMaxLogClients, "log");

  uint8_t buf[128];
  size_t got = 0;
  while (Serial2.available() > 0 && got < sizeof(buf)) {
    int b = Serial2.read();
    if (b < 0) break;
    buf[got++] = static_cast<uint8_t>(b);
  }
  if (got > 0) {
    totalRxBytes += got;
    lastRxMs = millis();
    broadcastRx(buf, got);
  }

  heartbeat();

  const bool shouldOn = (lastRxMs != 0) && (millis() - lastRxMs < kBlinkHoldMs);
  if (shouldOn != ledOn) {
    ledOn = shouldOn;
    if (shouldOn) {
      setLed(0, 200, 80);
    } else {
      setLed(0, 0, 0);
    }
  }
}
