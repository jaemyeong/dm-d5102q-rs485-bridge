# BOOT-RESET-MDNS-20260909

Approval: user requested `ATOM Lite의 버튼을 누르면서 전원을 인가하면 설정이 초기화 되도록 기능 구현 한뒤 업로드`,
then explicitly `mDNS도 포함해서 업로드` (2026-09-09 KST).
This additive B0 amendment preserves all original design, target, USB bootstrap
and web/AP amendment bytes. Build advances to usb-bootstrap-0.1.2 and includes
the previously host-only HTML login and unlimited provisioning AP.

## Boot-time Wi-Fi reset

The sole GPIO exception is sampling the C008 ATOM Lite front button on GPIO39
as INPUT, active LOW, before Wi-Fi initialization. No output, interrupt, internal
pull-up, Tail485 pin, bus initialization or packet transmission is permitted.
M5Stack's product pin map names GPIO39; its linked schematic shows S2 grounding
GPIO39 with R3 4.7k to 3V3. That schematic depicts a different USB interface from
the selected FTDI unit, so it is corroboration, not exact-revision verification.
ESP32 GPIO39 is input-only without an internal pull-up. Physical button behavior
on this selected board remains a separate acceptance observation.

Sources (checked 2026-09-09):
- https://docs.m5stack.com/en/core/ATOM%20Lite
- https://static-cdn.m5stack.com/resource/docs/products/core/ATOM%20Lite/img-2e58eac3-d9ef-4be4-b486-d4dd9a8324fa.webp
- https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/peripherals/gpio.html
- https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/system/system.html

A reset is armed only if esp_reset_reason is ESP_RST_POWERON and the front button
is already LOW at the initial application boot sample. Continuously sampled LOW
for at least 3000 ms requests reset once; any release cancels this boot's reset.
Late presses, software reboot, watchdog and brownout do not arm it. This is a
nonblocking, wrap-safe qualification gate; ordinary unpressed boot has no added
three-second delay. ESP32 cannot distinguish EN hard reset from power-on here:
holding the button through an EN reset can also qualify. Allow bootloader startup
time when operating it (hold approximately five seconds after applying power).

Reset removes only dmboot active/trial/cfgA/cfgB keys and resets the Wi-Fi config
revision. The per-device enroll key, unrelated keys and diagnostic NVS are kept.
Existing installation key validity is checked first. A fixed eight-byte versioned
reset-intent sentinel is committed/read back before deleting keys, which are
erased/read back idempotently. The sentinel is erased last. Startup resumes a
valid pending reset before loading any prior credentials; invalid markers or I/O
errors fail closed. Never erase the namespace, NVS partition or entire flash.
This is Wi-Fi recovery, not installation-key replacement or forensic flash wiping.
Reset requires re-entering Wi-Fi credentials; no automatic restoration is done.

## mDNS and authenticated web access

Use the bundled Espressif ESPmDNS in pinned Arduino-ESP32 2.0.17 (MIT), without
new library installation. Publish dm-bridge-<existing MAC suffix>.local and only
_http._tcp port 80 after STA IPv4 connectivity. Stop on disconnect, AP or fault;
failed start/service registration is cleaned up and retried at most once per
30 seconds. IP-based HTTP remains available if mDNS fails. No Arduino OTA service,
remote updater, TXT credentials or new public state endpoint is added.

Source: https://raw.githubusercontent.com/espressif/arduino-esp32/2.0.17/libraries/ESPmDNS/examples/mDNS_Web_Server/mDNS_Web_Server.ino

Host validation accepts only the actual local IPv4 address or this exact device's
.local hostname on STA, with optional port 80 and DNS case normalization. No
wildcards, arbitrary ports, alternate hosts or suffix matches. Configuration
writes retain exact normalized same-origin and CSRF validation before body reads.
Authenticated status includes hostname, mdnsUrl and mdnsActive. Public HTML is
still static, and existing authentication, replay and request bounds remain.
mDNS is LAN multicast, not a promise across routed VPN/subnets or client-isolated
Wi-Fi. Use the device's IP or existing private DNS over VPN. HTTP is not TLS.

## Upload and evidence boundary

The requested combined firmware is authorized for USB upload only to FTDI
0403:6001 / serial 2956578B12, ESP32-PICO-D4 v1.1, MAC 14:2b:2f:a1:10:88,
4 MiB. Re-identify before access/write, reject a busy port, retain a private
0600 device-digest-verified fresh full-flash backup at 115200, pin image hashes,
verify written bytes and preserved NVS. Keep the front button released during
upload. No installation key regeneration is expected on this upgrade.

Host reset interruption/security/lifecycle tests, target build, USB verification,
normal first boot, physical button hold and real LAN mDNS resolution are separate
evidence stages. Do not equate one with another. Existing partition layout and
1,310,720-byte application ceiling remain. No Tail485, 12V, Mac network/VPN changes,
OTA implementation or M0-M9/field/TX acceptance advancement is authorized here.
