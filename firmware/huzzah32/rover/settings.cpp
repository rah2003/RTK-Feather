#include "settings.h"

#include <Preferences.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

Settings g_settings;
static Preferences prefs;
static SemaphoreHandle_t s_mutex;

void settingsLock() { xSemaphoreTake(s_mutex, portMAX_DELAY); }
void settingsUnlock() { xSemaphoreGive(s_mutex); }

// NVS keys are short on purpose (15-char NVS limit) -- same convention as Metro's settings.cpp.
static void copyStr(char* dst, size_t dstLen, const String& src) {
  strncpy(dst, src.c_str(), dstLen - 1);
  dst[dstLen - 1] = '\0';
}

// Seeds an empty string field from a secrets.h macro, if that macro is defined. Only fires
// when NVS had nothing stored (first boot / after an erase) -- once set, NVS is authoritative
// and secrets.h is never consulted again this session or later.
static bool seedIfEmpty(char* dst, size_t dstLen, const char* fromMacro) {
  if (dst[0] || !fromMacro || !fromMacro[0]) return false;
  strncpy(dst, fromMacro, dstLen - 1);
  dst[dstLen - 1] = '\0';
  return true;
}

void settingsLoad() {
  if (!s_mutex) s_mutex = xSemaphoreCreateMutex();  // settingsLoad runs first in setup()
  prefs.begin("rtkfeather", true);
  for (int i = 0; i < kMaxWifiNetworks; i++) {
    copyStr(g_settings.wifiSsid[i], sizeof(g_settings.wifiSsid[i]),
            prefs.getString((String("ssid") + i).c_str(), ""));
    copyStr(g_settings.wifiPass[i], sizeof(g_settings.wifiPass[i]),
            prefs.getString((String("wpass") + i).c_str(), ""));
  }
  copyStr(g_settings.casterHost, sizeof(g_settings.casterHost),
          prefs.getString("chost", g_settings.casterHost));
  g_settings.casterPort = prefs.getUShort("cport", g_settings.casterPort);
  copyStr(g_settings.casterMount, sizeof(g_settings.casterMount),
          prefs.getString("cmount", g_settings.casterMount));
  copyStr(g_settings.casterUser, sizeof(g_settings.casterUser), prefs.getString("cuser", ""));
  copyStr(g_settings.casterPass, sizeof(g_settings.casterPass), prefs.getString("cpass", ""));
  uint16_t gp = prefs.getUShort("ggaper", g_settings.ggaPeriodS);
  g_settings.ggaPeriodS = gp ? gp : 10;  // 0 would flood the caster with GGA every loop pass
  g_settings.mode = (DeviceMode)prefs.getUChar("mode", 0);
  g_settings.elevMaskDeg = prefs.getUChar("elevmask", g_settings.elevMaskDeg);
  g_settings.logUbx = prefs.getBool("logubx", true);
  prefs.end();

  // First-boot / post-erase convenience: pull WiFi + caster credentials from a gitignored
  // secrets.h if present and NVS didn't already have them (see secrets.example.h).
  bool seeded = false;
#ifdef RTKF_WIFI_SSID1
  seeded |= seedIfEmpty(g_settings.wifiSsid[0], sizeof(g_settings.wifiSsid[0]), RTKF_WIFI_SSID1);
#endif
#ifdef RTKF_WIFI_PASS1
  seeded |= seedIfEmpty(g_settings.wifiPass[0], sizeof(g_settings.wifiPass[0]), RTKF_WIFI_PASS1);
#endif
#ifdef RTKF_CASTER_USER
  seeded |= seedIfEmpty(g_settings.casterUser, sizeof(g_settings.casterUser), RTKF_CASTER_USER);
#endif
#ifdef RTKF_CASTER_PASS
  seeded |= seedIfEmpty(g_settings.casterPass, sizeof(g_settings.casterPass), RTKF_CASTER_PASS);
#endif
  if (seeded) {
    Serial.println(F("[cfg] seeded credentials from secrets.h (first boot) -- saving to NVS"));
    settingsSave();
  }
}

void settingsSave() {
  prefs.begin("rtkfeather", false);
  for (int i = 0; i < kMaxWifiNetworks; i++) {
    prefs.putString((String("ssid") + i).c_str(), g_settings.wifiSsid[i]);
    prefs.putString((String("wpass") + i).c_str(), g_settings.wifiPass[i]);
  }
  prefs.putString("chost", g_settings.casterHost);
  prefs.putUShort("cport", g_settings.casterPort);
  prefs.putString("cmount", g_settings.casterMount);
  prefs.putString("cuser", g_settings.casterUser);
  prefs.putString("cpass", g_settings.casterPass);
  prefs.putUShort("ggaper", g_settings.ggaPeriodS);
  prefs.putUChar("mode", (uint8_t)g_settings.mode);
  prefs.putUChar("elevmask", g_settings.elevMaskDeg);
  prefs.putBool("logubx", g_settings.logUbx);
  prefs.end();
}

// Tiny RAII guard so the many early returns below can't leak the lock.
struct SettingsGuard {
  SettingsGuard() { settingsLock(); }
  ~SettingsGuard() { settingsUnlock(); }
};

bool settingsApplyKeyValue(const char* key, const char* value) {
  SettingsGuard guard;  // writers lock; cross-core string readers copy out under the lock
  if (strncmp(key, "wifi", 4) == 0 && key[4] >= '1' && key[4] <= '0' + kMaxWifiNetworks) {
    int i = key[4] - '1';
    strncpy(g_settings.wifiSsid[i], value, sizeof(g_settings.wifiSsid[i]) - 1);
    g_settings.wifiSsid[i][sizeof(g_settings.wifiSsid[i]) - 1] = '\0';
    return true;
  }
  if (strncmp(key, "pass", 4) == 0 && key[4] >= '1' && key[4] <= '0' + kMaxWifiNetworks) {
    int i = key[4] - '1';
    strncpy(g_settings.wifiPass[i], value, sizeof(g_settings.wifiPass[i]) - 1);
    g_settings.wifiPass[i][sizeof(g_settings.wifiPass[i]) - 1] = '\0';
    return true;
  }
  if (!strcmp(key, "caster")) {
    strncpy(g_settings.casterHost, value, sizeof(g_settings.casterHost) - 1);
    g_settings.casterHost[sizeof(g_settings.casterHost) - 1] = '\0';
    return true;
  }
  if (!strcmp(key, "port")) { g_settings.casterPort = atoi(value); return true; }
  if (!strcmp(key, "mount")) {
    strncpy(g_settings.casterMount, value, sizeof(g_settings.casterMount) - 1);
    g_settings.casterMount[sizeof(g_settings.casterMount) - 1] = '\0';
    return true;
  }
  if (!strcmp(key, "user")) {
    strncpy(g_settings.casterUser, value, sizeof(g_settings.casterUser) - 1);
    g_settings.casterUser[sizeof(g_settings.casterUser) - 1] = '\0';
    return true;
  }
  if (!strcmp(key, "password")) {
    strncpy(g_settings.casterPass, value, sizeof(g_settings.casterPass) - 1);
    g_settings.casterPass[sizeof(g_settings.casterPass) - 1] = '\0';
    return true;
  }
  if (!strcmp(key, "ggaperiod")) {
    g_settings.ggaPeriodS = constrain(atoi(value), 1, 3600);  // 0 = GGA flood; clamp it out
    return true;
  }
  if (!strcmp(key, "elevmask")) { g_settings.elevMaskDeg = constrain(atoi(value), 0, 45); return true; }
  if (!strcmp(key, "logubx")) { g_settings.logUbx = strcasecmp(value, "off") != 0; return true; }
  return false;
}

void settingsPrint(Stream& out) {
  out.println(F("--- settings ---"));
  for (int i = 0; i < kMaxWifiNetworks; i++)
    if (g_settings.wifiSsid[i][0])
      out.printf("wifi%d      = %s (pass %s)\n", i + 1, g_settings.wifiSsid[i],
                 g_settings.wifiPass[i][0] ? "***" : "<empty>");
  out.printf("caster     = %s:%u /%s user=%s pass=%s\n", g_settings.casterHost,
             g_settings.casterPort, g_settings.casterMount, g_settings.casterUser,
             g_settings.casterPass[0] ? "***" : "<empty>");
  out.printf("mode       = %s\n", g_settings.mode == DeviceMode::Base ? "base" : "rover");
  out.printf("elevmask   = %u deg\n", g_settings.elevMaskDeg);
  out.printf("logubx     = %s\n", g_settings.logUbx ? "on" : "off");
}
