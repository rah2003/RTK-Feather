// Device configuration, persisted in ESP32 NVS. Ported from the Metro project's settings.h
// (rah2003/SWMaps-propertylines) and trimmed for v1: Base-mode fields (survey-in/fixed
// position) are deliberately NOT carried over yet -- this build is Rover-only until Phase 3,
// and copying an unused schema now just invites drift. Add them back when Base mode is
// actually built.
//
// Unlike Metro (which has an onboard microSD its ESP32 can read directly), the HUZZAH32 has
// no SD slot -- only the M0 does, and the M0 is not the settings owner. So the Metro
// "SD /config.txt wins over NVS" mechanism doesn't have anywhere to live on this board; NVS
// + the serial menu + an optional compiled-in secrets.h seed (secrets.example.h) stand in
// for it here. See docs/QUESTIONS.md Q1 discussion and secrets.example.h.
#pragma once
#include <Arduino.h>

constexpr int kMaxWifiNetworks = 4;

enum class DeviceMode : uint8_t { Rover = 0, Base = 1 };

struct Settings {
  // --- WiFi (tried in order) ---
  char wifiSsid[kMaxWifiNetworks][33] = {{0}};
  char wifiPass[kMaxWifiNetworks][65] = {{0}};

  // --- NTRIP caster --- defaults inherited wholesale from the Metro project's answered
  // Section-10-equivalent questions (2026-07-08): same caster/WiFi source for this build.
  char casterHost[65] = "acorn-gnss.net";
  uint16_t casterPort = 2101;
  char casterMount[49] = "VRS_SouthCentral_RTCM3";  // alt: MS_RTCM3 (non-VRS mount)
  char casterUser[49] = {0};                        // never defaulted -- see secrets.example.h
  char casterPass[49] = {0};
  uint16_t ggaPeriodS = 10;  // GGA upstream cadence (VRS requirement)

  // --- Mode --- Rover-only for v1 (FEATURE_BASE=0); field kept for forward compatibility.
  DeviceMode mode = DeviceMode::Rover;

  // --- GNSS --- inherited default (Metro Q11); docs/hardware/ucenter-config.md's own bench
  // checklist proposed 10 deg as a starting point -- this build uses the inherited 12 deg
  // per project decision.
  uint8_t elevMaskDeg = 12;

  // --- Logging --- whether the M0 should be logging at all. "Always-on" (Metro Q6 pattern):
  // true from boot; the OLED-Wing buttons can still toggle it (project brief §9 button UX).
  // Relayed to the M0 over the status link, not read by the M0 from its own storage (it has
  // none). Fix-quality CSV logging (brief §9 usability suggestion) is NOT implemented in v1
  // -- it's explicitly a "propose, don't silently implement" item; flagged, not built.
  bool logUbx = true;
};

extern Settings g_settings;

void settingsLoad();  // NVS -> g_settings (seeds from secrets.h on first boot); creates the mutex
void settingsSave();  // g_settings -> NVS
void settingsPrint(Stream& out);                                 // dump (passwords masked)
bool settingsApplyKeyValue(const char* key, const char* value);  // used by the serial menu

// The serial menu mutates g_settings strings at runtime while ntripTask (other core) reads
// them; writes take this lock internally (settingsApplyKeyValue), and cross-core readers of
// the string fields must copy them out under it. Scalars (enums/ints/bools) are read
// lock-free -- a torn 1-4 byte read can't happen on this architecture.
void settingsLock();
void settingsUnlock();
