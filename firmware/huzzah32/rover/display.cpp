// 128x32 SSD1306 rendering. Constructor/init per Adafruit's own FeatherWing example
// (Adafruit_SSD1306/examples/OLED_featherwing: Adafruit_SSD1306(128, 32, &Wire) +
// begin(SSD1306_SWITCHCAPVCC, 0x3C)). Page content follows the project brief §3.5 and
// docs/field-guide.md, compressed to the panel's 21 col x 4 row budget (6x8 font).
#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include "features.h"
#include "ntrip_client.h"
#include "pins.h"
#include "shared.h"
#include "ui_task.h"

namespace {

Adafruit_SSD1306 oled(128, 32, &Wire);
bool oledPresent = false;

uint32_t lastRenderMs = 0;
uint32_t lastSerialMs = 0;

// bytes/s for the NTRIP page: delta of g_link.rtcmBytes over a sliding ~2 s window.
uint32_t lastRateSampleMs = 0;
uint32_t lastRateBytes = 0;
uint32_t rtcmBytesPerSec = 0;

constexpr uint32_t kCorrectionAlarmMs = 10000;  // brief §3.2: age >10 s = alarm

const char* fixTypeName(const GnssStatus& gs) {
  // NAV-PVT fixType + carrSoln, compressed for a 21-char line. RTK states win: they're
  // what the operator actually watches (Emlid-style status-at-a-glance, brief §6).
  if (gs.carrSoln == 2) return "RTK FIX";
  if (gs.carrSoln == 1) return "RTK FLT";
  switch (gs.fixType) {
    case 2: return "2D";
    case 3: return "3D";
    case 4: return "GNSS+DR";
    default: return "NO FIX";
  }
}

void line(int row, const char* text) {
  oled.setCursor(0, row * 8);
  oled.print(text);
}

// Inverse-video line: filled bar, black text -- the mono-panel stand-in for a red alert
// (brief §9). Used by the NTRIP page's correction-age alarm, flashed on odd half-seconds.
void lineInverse(int row, const char* text) {
  oled.fillRect(0, row * 8, 128, 8, SSD1306_WHITE);
  oled.setTextColor(SSD1306_BLACK);
  oled.setCursor(0, row * 8);
  oled.print(text);
  oled.setTextColor(SSD1306_WHITE);
}

void pageFix(const GnssStatus& gs) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%-8s sats %u", fixTypeName(gs), gs.numSV);
  line(0, buf);
  snprintf(buf, sizeof(buf), "lat %11.7f", gs.latDeg);
  line(1, buf);
  snprintf(buf, sizeof(buf), "lon %11.7f", gs.lonDeg);
  line(2, buf);
  // Brief asks for HDOP; NAV-PVT carries pDOP (and hAcc, which field crews watch more) --
  // show both, labeled honestly.
  if (gs.hAccMm < 10000) {
    snprintf(buf, sizeof(buf), "hAcc %lumm pDOP %.1f", (unsigned long)gs.hAccMm, gs.pdop);
  } else {
    snprintf(buf, sizeof(buf), "hAcc --   pDOP %.1f", gs.pdop);
  }
  line(3, buf);
}

void pageNtrip(uint32_t nowMs) {
  char buf[24];
  // %.15s truncates ("caster-connecting" is 17 chars; %-15s alone would pad but NOT cut).
  snprintf(buf, sizeof(buf), "NTRIP %.15s", ntripStateName());
  line(0, buf);
  uint32_t age = correctionAgeMs();
  if (age == UINT32_MAX) {
    line(1, "age  --");
  } else {
    snprintf(buf, sizeof(buf), "age  %.1fs", age / 1000.0f);
    // Alarm: inverse-video flash (on during odd half-seconds) once corrections go stale.
    if (age > kCorrectionAlarmMs && (nowMs / 500) % 2) {
      lineInverse(1, buf);
    } else {
      line(1, buf);
    }
  }
  snprintf(buf, sizeof(buf), "rtcm %lu B/s", (unsigned long)rtcmBytesPerSec);
  line(2, buf);
  if (g_link.wifiUp) {
    snprintf(buf, sizeof(buf), "wifi %d dBm", (int)g_link.wifiRssi);
  } else {
    snprintf(buf, sizeof(buf), "wifi down");
  }
  line(3, buf);
}

void pageLogging(const M0Status& m0) {
  char buf[24];
  snprintf(buf, sizeof(buf), "LOG %s  link %s", m0.logging ? "on " : "off",
           m0.linkUp ? "up" : "DOWN");
  line(0, buf);
  // Basename only: the full /YYYYMMDD/r_HHMMSS.ubx is 22 chars, one over the panel width,
  // and the date directory is redundant on a status page.
  const char* base = strrchr(m0.fileName, '/');
  base = base ? base + 1 : m0.fileName;
  line(1, base[0] ? base : "(no file)");
  snprintf(buf, sizeof(buf), "%.1f MB written", m0.bytesWritten / 1048576.0f);
  line(2, buf);
  snprintf(buf, sizeof(buf), "free %.1f GB", m0.sdFreeKB / 1048576.0f);
  line(3, buf);
}

void pageBle() {
  line(0, "BLE");
#if FEATURE_BLE
  line(1, "enabled");  // placeholder: real state once the Phase 4 BLE port lands
#else
  line(1, "off (v1: WiFi-only)");
  line(2, "Phase 4 feature");
#endif
}

void render(uint32_t nowMs) {
  oled.clearDisplay();
  oled.setTextSize(1);  // 6x8 -> 21 cols x 4 rows
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextWrap(false);  // overlong lines clip at the right edge, never corrupt the next row

  GnssStatus gs = statusGetGnss();
  M0Status m0 = statusGetM0();

  switch (uiCurrentPage()) {
    case 0: pageFix(gs); break;
    case 1: pageNtrip(nowMs); break;
    case 2: pageLogging(m0); break;
    default: pageBle(); break;
  }
  oled.display();
}

}  // namespace

void displayInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  // begin() allocates the 512 B framebuffer and probes the panel; a missing Wing (or absent
  // pullups -- the HUZZAH32 has none of its own, checklists.md) fails here and we run
  // headless rather than wedging the boot.
  oledPresent = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledPresent) {
    Serial.println(F("[display] SSD1306 not found at 0x3C -- running headless"));
    return;
  }
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 8);
  oled.println(F("RTK-Feather Rover"));
  oled.println(F("build " __DATE__));
  oled.display();
  Serial.println(F("[display] SSD1306 128x32 up"));
}

void displayUpdate() {
  uint32_t now = millis();

  // rtcm B/s sample, ~2 s window (independent of render cadence).
  if (now - lastRateSampleMs >= 2000) {
    uint32_t bytes = g_link.rtcmBytes;
    rtcmBytesPerSec = (bytes - lastRateBytes) / ((now - lastRateSampleMs) / 1000);
    lastRateBytes = bytes;
    lastRateSampleMs = now;
  }

  // OLED at 4 Hz -- fast enough for the 2 Hz alarm flash to look steady, slow enough to
  // keep the I2C bus (shared with nothing else, but still) and CPU quiet.
  if (oledPresent && now - lastRenderMs >= 250) {
    lastRenderMs = now;
    render(now);
  }

  // Serial status line every 2 s -- bench/headless visibility, same fields as before.
  if (now - lastSerialMs >= 2000) {
    lastSerialMs = now;
    GnssStatus gs = statusGetGnss();
    M0Status m0 = statusGetM0();
    uint32_t age = correctionAgeMs();
    char ageStr[16];
    if (age == UINT32_MAX) {
      strcpy(ageStr, "never");
    } else {
      snprintf(ageStr, sizeof(ageStr), "%.1fs", age / 1000.0f);
    }
    Serial.printf("[status] fix=%u carr=%u sv=%u ntrip=%s corrAge=%s logging=%s file=%s\n",
                  gs.fixType, gs.carrSoln, gs.numSV, ntripStateName(), ageStr,
                  m0.logging ? "on" : "off", m0.fileName[0] ? m0.fileName : "-");
  }
}
