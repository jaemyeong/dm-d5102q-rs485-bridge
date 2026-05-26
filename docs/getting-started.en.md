# Getting Started
> 🌐 Read this in: [한국어](./getting-started.md) · **English**

## Requirements

- M5Stack AtomS3-Lite or Atom-Lite
- Atom RS485 Base
- USB-C cable and stable 5V power
- Arduino IDE 2.x
- A user-owned DM-D5102Q wall pad or isolated RS485 test environment

## Arduino IDE Setup

1. Install Arduino IDE 2.x.
2. Add `https://espressif.github.io/arduino-esp32/package_esp32_index.json` to the board manager URLs.
3. Install the ESP32 board package.
4. Select the target board.
   - AtomS3-Lite: M5Stack AtomS3 family board
   - Atom-Lite: M5Stack Atom family board

## Libraries

Install these libraries through Library Manager or ZIP packages.

| Library | Purpose |
|---|---|
| `M5Atom` | Atom-Lite board support |
| `M5AtomS3` | AtomS3-Lite board support |
| `WiFi` | ESP32 Wi-Fi |
| `ESPAsyncWebServer` | HTTP/WebSocket server |
| `AsyncTCP` | Async TCP foundation |
| `Preferences` | NVS settings storage |
| `ElegantOTA` | OTA uploads |
| `ArduinoJson` | API JSON serialization |

## First Build Flow

1. Copy `firmware/<board>/secrets.h.example` to `secrets.h`. This file will be added in a later implementation phase.
2. Upload `firmware/<board>/data/` through the LittleFS uploader.
3. Compile and upload the board-specific sketch.
4. On first boot, the device enters AP mode when no saved Wi-Fi credentials exist.
5. Open the web browser and check the settings form, console, and dashboard.

## First login

Starting with 0.1.9, every web access is protected with HTTP Basic Auth.

1. After connecting to the AP, opening `http://192.168.4.1/` triggers a browser auth prompt (`realm: "DM-D5102Q Bridge"`).
2. Default credentials are username `admin`, password `admin`. If `secrets.h` defines `BRIDGE_DEFAULT_WEB_USER` and `BRIDGE_DEFAULT_WEB_PASSWORD`, those values apply instead.
3. After logging in, change the password immediately in the Admin UI > Settings > Security section.
4. After switching to STA mode, the new IP will prompt for credentials again.

## Safety Notes

- Confirm wall pad power and common ground before connecting RS485 A/B/GND.
- HEX TX sends frames onto a real bus; use it only with equipment you own.
- Do not expose the device directly to the internet.

## TODO

- Add exact board FQBNs and LittleFS uploader screenshots.
- Decide default first-boot AP SSID/password values.
