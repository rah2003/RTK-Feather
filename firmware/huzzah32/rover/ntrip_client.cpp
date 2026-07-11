// NTRIP client state machine. Ported near-verbatim from the Metro project's ntrip_client.cpp
// (rah2003/SWMaps-propertylines) -- this file has no dependency on GNSS transport (I2C vs
// UART1), Base/Rover mode, or logging, so it needed essentially no adaptation for Topology B.
// Pattern borrowed (via Metro) from SparkFun RTK Everywhere's NtripClient (states +
// exponential backoff + GGA upstream) and ArduSimple's ESP32-XBee GGA cadence.
//
// Rules this file lives by:
//  - every network operation has a timeout; nothing here can block forever
//  - the task is registered with the ESP task watchdog -- a wedged socket reboots the box
//    rather than silently dying in the field
//  - rev1 ("ICY 200 OK") and rev2 ("HTTP/1.x 200") responses both accepted (this project's
//    caster, acorn-gnss.net, and its VRS_SouthCentral_RTCM3 mount -- Metro's answered Q1)
//  - correction-age watchdog: >10 s with no RTCM while "connected" tears the socket down
#include "ntrip_client.h"

#include <WiFi.h>
#include <esp_task_wdt.h>

#include "features.h"
#include "settings.h"
#include "shared.h"

#if FEATURE_NTRIP

namespace {

enum class NtripState { WifiConnecting, CasterConnecting, Connected, WaitRetry };
NtripState state = NtripState::WifiConnecting;

WiFiClient sock;
uint32_t retryDelayMs = 1000;  // exponential backoff: 1s -> 60s cap
constexpr uint32_t kRetryMaxMs = 60000;
constexpr uint32_t kSocketTimeoutMs = 5000;
constexpr uint32_t kCorrectionStaleMs = 10000;
uint32_t stateEnteredMs = 0;
uint32_t lastGgaSentMs = 0;

const char* kStateNames[] = {"wifi-connecting", "caster-connecting", "connected", "wait-retry"};

void enter(NtripState s) {
  state = s;
  stateEnteredMs = millis();
}

void backoff() {
  sock.stop();
  g_link.ntripConnected = false;
  enter(NtripState::WaitRetry);
}

// Base64 for Basic auth, no heap churn. Bytes must be widened UNSIGNED: char is signed on
// Xtensa, and a credential byte >= 0x80 would sign-extend and corrupt the neighboring 6-bit
// groups -- a permanent, undiagnosable 401.
void base64enc(const char* in, char* out, size_t outLen) {
  static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t n = strlen(in), o = 0;
  for (size_t i = 0; i < n && o + 4 < outLen; i += 3) {
    uint32_t v = (uint32_t)(uint8_t)in[i] << 16 |
                 (i + 1 < n ? (uint32_t)(uint8_t)in[i + 1] << 8 : 0) |
                 (i + 2 < n ? (uint32_t)(uint8_t)in[i + 2] : 0);
    out[o++] = tbl[(v >> 18) & 63];
    out[o++] = tbl[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? tbl[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? tbl[v & 63] : '=';
  }
  out[o] = '\0';
}

bool wifiTryConnect() {
  for (int i = 0; i < kMaxWifiNetworks; i++) {
    // Snapshot under the settings lock: the serial menu can rewrite these strings from the
    // other core mid-connect, and a torn SSID/password read looks like a bad credential.
    char ssid[33], pass[65];
    settingsLock();
    strncpy(ssid, g_settings.wifiSsid[i], sizeof(ssid) - 1); ssid[sizeof(ssid) - 1] = '\0';
    strncpy(pass, g_settings.wifiPass[i], sizeof(pass) - 1); pass[sizeof(pass) - 1] = '\0';
    settingsUnlock();
    if (!ssid[0]) continue;
    Serial.printf("[ntrip] WiFi: trying %s\n", ssid);
    WiFi.begin(ssid, pass);
    uint32_t t0 = millis();
    while (millis() - t0 < 12000) {  // hotspots are slow to answer; 12 s per SSID
      if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[ntrip] WiFi up: %s  RSSI %d\n", WiFi.localIP().toString().c_str(),
                      WiFi.RSSI());
        return true;
      }
      esp_task_wdt_reset();
      vTaskDelay(pdMS_TO_TICKS(250));
    }
    WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(500));
  }
  return false;
}

bool casterConnect() {
  // Snapshot caster config under the settings lock (menu edits race these strings).
  char host[65], mount[49], user[49], pass[49];
  uint16_t port;
  settingsLock();
  strncpy(host, g_settings.casterHost, sizeof(host) - 1); host[sizeof(host) - 1] = '\0';
  strncpy(mount, g_settings.casterMount, sizeof(mount) - 1); mount[sizeof(mount) - 1] = '\0';
  strncpy(user, g_settings.casterUser, sizeof(user) - 1); user[sizeof(user) - 1] = '\0';
  strncpy(pass, g_settings.casterPass, sizeof(pass) - 1); pass[sizeof(pass) - 1] = '\0';
  port = g_settings.casterPort;
  settingsUnlock();

  if (!host[0] || !mount[0]) {
    Serial.println(F("[ntrip] no caster configured (set caster/port/mount, or via secrets.h)"));
    return false;
  }
  sock.setTimeout(kSocketTimeoutMs / 1000);
  if (!sock.connect(host, port, kSocketTimeoutMs)) {
    Serial.printf("[ntrip] TCP connect to %s:%u failed\n", host, port);
    return false;
  }

  char auth[100] = {0}, authB64[140] = {0};
  if (user[0]) {
    snprintf(auth, sizeof(auth), "%s:%s", user, pass);
    base64enc(auth, authB64, sizeof(authB64));
  }

  // Rev2-style request; rev1 casters ignore the extra headers and answer "ICY 200 OK".
  char req[512];
  int n = snprintf(req, sizeof(req),
                   "GET /%s HTTP/1.1\r\n"
                   "Host: %s:%u\r\n"
                   "Ntrip-Version: Ntrip/2.0\r\n"
                   "User-Agent: NTRIP RTKFeather/1.0\r\n"
                   "Accept: */*\r\n"
                   "Connection: close\r\n",
                   mount, host, port);
  if (authB64[0])
    n += snprintf(req + n, sizeof(req) - n, "Authorization: Basic %s\r\n", authB64);
  n += snprintf(req + n, sizeof(req) - n, "\r\n");
  sock.write((const uint8_t*)req, n);

  // Read the response header line(s) with a hard deadline.
  char line[256];
  size_t li = 0;
  bool ok = false, headerDone = false;
  uint32_t t0 = millis();
  while (millis() - t0 < kSocketTimeoutMs && !headerDone) {
    if (!sock.connected()) break;
    // headerDone must stop THIS loop too: after "ICY 200 OK" the very next buffered bytes
    // are binary RTCM, and reading on would chew them as fake header lines.
    while (sock.available() && !headerDone) {
      char c = sock.read();
      if (c == '\n') {
        line[li] = '\0';
        if (li <= 1) { headerDone = true; break; }  // blank line = end of headers
        if (strstr(line, "ICY 200 OK")) {
          // NTRIP rev1: no further headers -- RTCM starts immediately. Stop reading lines
          // now or we'd chew binary data (and 5 s of corrections) hunting for a blank line.
          ok = true;
          headerDone = true;
        }
        if (strstr(line, "HTTP/1.1 200") || strstr(line, "HTTP/1.0 200"))
          ok = true;  // rev2: keep reading to the blank line that ends the headers
        if (strstr(line, "SOURCETABLE")) {
          // Caster answered with the sourcetable: mountpoint name is wrong.
          Serial.printf("[ntrip] mountpoint '%s' not found (got SOURCETABLE)\n", mount);
          ok = false;
          headerDone = true;
        }
        if (strstr(line, "401") || strstr(line, "403"))
          Serial.println(F("[ntrip] caster rejected credentials"));
        li = 0;
      } else if (c != '\r' && li < sizeof(line) - 1) {
        line[li++] = c;
      }
    }
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
  if (!ok) {
    sock.stop();
    return false;
  }
  Serial.println(F("[ntrip] caster connected, RTCM flowing"));
  return true;
}

// Forward the latest GGA upstream every ggaPeriodS. Required by VRS/network mountpoints
// (this project's default is VRS_SouthCentral_RTCM3) so the caster can synthesize
// corrections for our location; harmless on physical mounts like MS_RTCM3.
void maybeSendGga() {
  if (millis() - lastGgaSentMs < (uint32_t)g_settings.ggaPeriodS * 1000) return;
  GnssStatus s = statusGetGnss();
  if (!s.lastGga[0]) return;
  sock.print(s.lastGga);  // sentence already includes \r\n
  lastGgaSentMs = millis();
}

void ntripTask(void*) {
  esp_task_wdt_add(nullptr);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // modem sleep adds 100s of ms latency to RTCM; we have wall power

  for (;;) {
    esp_task_wdt_reset();
    g_link.wifiUp = (WiFi.status() == WL_CONNECTED);
    g_link.wifiRssi = g_link.wifiUp ? (int8_t)WiFi.RSSI() : 0;  // for the OLED NTRIP page

    switch (state) {
      case NtripState::WifiConnecting:
        if (wifiTryConnect()) enter(NtripState::CasterConnecting);
        else enter(NtripState::WaitRetry);
        break;

      case NtripState::CasterConnecting:
        if (WiFi.status() != WL_CONNECTED) { enter(NtripState::WifiConnecting); break; }
        if (casterConnect()) {
          g_link.ntripConnected = true;
          g_link.lastRtcmMs = millis();  // grace period before the stale watchdog can fire
          retryDelayMs = 1000;           // success resets the backoff
          lastGgaSentMs = 0;
          enter(NtripState::Connected);
        } else {
          backoff();
        }
        break;

      case NtripState::Connected: {
        if (WiFi.status() != WL_CONNECTED || !sock.connected()) {
          Serial.println(F("[ntrip] link lost"));
          backoff();
          break;
        }
        uint8_t buf[512];
        while (sock.available()) {
          int n = sock.read(buf, sizeof(buf));
          if (n <= 0) break;
          size_t w = rtcmRing.write(buf, n);
          if (w < (size_t)n)
            Serial.println(F("[ntrip] rtcmRing overflow (gnssTask stalled?)"));
          g_link.lastRtcmMs = millis();
          g_link.rtcmBytes += n;
        }
        if (millis() - g_link.lastRtcmMs > kCorrectionStaleMs) {
          // Socket "connected" but silent -- half-open TCP is common on hotspot handoffs.
          Serial.println(F("[ntrip] corrections stale >10 s, reconnecting"));
          backoff();
          break;
        }
        maybeSendGga();
        vTaskDelay(pdMS_TO_TICKS(10));
        break;
      }

      case NtripState::WaitRetry:
        if (millis() - stateEnteredMs >= retryDelayMs) {
          retryDelayMs = min(retryDelayMs * 2, kRetryMaxMs);
          enter(WiFi.status() == WL_CONNECTED ? NtripState::CasterConnecting
                                              : NtripState::WifiConnecting);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        break;
    }
  }
}

}  // namespace

void ntripTaskStart() {
  xTaskCreatePinnedToCore(ntripTask, "ntrip", 8192, nullptr, 2, nullptr, 0 /* core 0 */);
}

const char* ntripStateName() { return kStateNames[(int)state]; }

#else
void ntripTaskStart() {}
const char* ntripStateName() { return "disabled"; }
#endif  // FEATURE_NTRIP
