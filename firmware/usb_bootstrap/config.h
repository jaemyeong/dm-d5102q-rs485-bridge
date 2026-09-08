#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef ARDUINO
#include <sdkconfig.h>
#ifdef DM_HOST_TEST
#error "Host fixture identity must never be compiled into device firmware"
#endif
#endif

#if defined(ARDUINO) && (!defined(CONFIG_IDF_TARGET_ESP32) || \
    (!defined(ARDUINO_M5Stack_ATOM) && !defined(ARDUINO_M5STACK_ATOM)))
#error "USB bootstrap supports only the pinned ESP32 M5Stack ATOM Lite target"
#endif

namespace bootstrap {
#ifndef DM_BUILD_ID
#define DM_BUILD_ID "usb-bootstrap-0.3.0"
#endif
#ifndef DM_OTA_VERSION
#define DM_OTA_VERSION 300
#endif
constexpr char kBuildId[] = DM_BUILD_ID;
#define DM_STRING_IMPL(x) #x
#define DM_STRING(x) DM_STRING_IMPL(x)
constexpr char kReleaseTag[] = "DMOTA-RELEASE:" DM_BUILD_ID ":" DM_STRING(DM_OTA_VERSION);
constexpr char kUpdaterTag[] = "DMOTA-UPDATER:2";
#ifndef DM_GITHUB_AUTO_UPDATE
#define DM_GITHUB_AUTO_UPDATE 0
#endif
static_assert(DM_GITHUB_AUTO_UPDATE == 0 || DM_GITHUB_AUTO_UPDATE == 1, "Explicit GitHub automatic-update policy");
// G6 activation requires separate recovery/rollout evidence. Manual authenticated
// GitHub check-and-update remains available for bounded bench tests after G4.
constexpr bool kGithubAutomatic = DM_GITHUB_AUTO_UPDATE == 1;
constexpr uint32_t kOtaVersion = DM_OTA_VERSION;
constexpr uint32_t kConfigSchema = 1;
constexpr size_t kUploadMax = 1310720;
static_assert(sizeof(kBuildId) <= 48, "OTA build identity limit");
constexpr char kRealm[] = "DM-BRIDGE-USB";
constexpr char kUsername[] = "installer";
constexpr size_t kHeaderMax = 2048;
constexpr size_t kBodyMax = 1024;
constexpr size_t kRecordBytes = 112;
constexpr size_t kReferenceBytes = 13;
// WEB-LOGIN-AP-20260909: provisioning ends by state transition, not elapsed time.
constexpr uint32_t kTrialMs = 60000;
constexpr uint32_t kStableMs = 5000;
constexpr uint32_t kRequestMs = 5000;
constexpr uint32_t kNonceMs = 300000;
constexpr uint32_t kRebootMs = 1000;
constexpr size_t kIoPerLoop = 256;
constexpr uint32_t kBootResetMs = 3000;
constexpr uint32_t kMdnsRetryMs = 30000;
// Only boot_button.h may sample the onboard button; bus drivers remain absent.
}
