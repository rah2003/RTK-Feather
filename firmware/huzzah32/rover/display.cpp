#include "display.h"

#include <Wire.h>

#include "ntrip_client.h"
#include "pins.h"
#include "shared.h"

namespace {
uint32_t lastPrintMs = 0;
}

void displayInit() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  // No display library initialized yet -- Q2 unresolved (see header). This I2C scan mirrors
  // the Phase 1 bring-up sketch's check, just so a missing/wrong-address Wing is visible on
  // the console even without a real driver.
  bool found = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[display] I2C device at 0x%02X%s\n", addr,
                    addr == 0x3C ? "  <-- OLED (expected)" : "");
      if (addr == 0x3C) found = true;
    }
  }
  if (!found) Serial.println(F("[display] no device at 0x3C -- OLED Wing not detected"));
}

void displayUpdate() {
  if (millis() - lastPrintMs < 2000) return;
  lastPrintMs = millis();
  GnssStatus gs = statusGetGnss();
  M0Status m0 = statusGetM0();
  uint32_t age = correctionAgeMs();
  Serial.printf("[status] fix=%u carr=%u sv=%u ntrip=%s corrAge=%s logging=%s file=%s\n",
                gs.fixType, gs.carrSoln, gs.numSV, ntripStateName(),
                age == UINT32_MAX ? "never" : (String(age / 1000.0f, 1) + "s").c_str(),
                m0.logging ? "on" : "off", m0.fileName[0] ? m0.fileName : "-");
}
