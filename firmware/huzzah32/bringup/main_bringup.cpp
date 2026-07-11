#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Preferences.h>
#include <Wire.h>

#include "fake_nmea.h"
#include "pins.h"

// Phase 1 bring-up sketch (docs/test-plan.md Stage 1 item 1) + fake NMEA source (item 2
// onward). No WiFi/NTRIP/BLE here yet -- this only proves the board, the stacked OLED
// Wing's I2C bus and panel (128x32 SSD1306, Q2 answered), the three buttons, and NVS
// survive a cycle, and it stands in for the F9P on Serial1 so later stages (NTRIP soak,
// BLE bridge) have something to consume.
// FAKE_NMEA_SOURCE is set by platformio.ini (default 1: no F9P on the bench yet).

FakeNmeaSource fakeNmea;
Preferences prefs;
Adafruit_SSD1306 oled(128, 32, &Wire);  // constructor per Adafruit's OLED_featherwing example
bool oledUp = false;

static void i2cScan() {
  Serial.println(F("[i2c] scanning..."));
  bool foundOled = false;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[i2c] device at 0x%02X%s\n", addr, addr == 0x3C ? "  <-- OLED (expected)" : "");
      if (addr == 0x3C) foundOled = true;
    }
  }
  if (!foundOled) {
    Serial.println(F(
        "[i2c] WARNING: no device at 0x3C -- OLED Wing not detected "
        "(checklists.md: HUZZAH32 has no onboard I2C pullups, the Wing supplies them)"));
  }
}

static void nvsRoundTrip() {
  prefs.begin("bringup", false);
  uint32_t bootCount = prefs.getUInt("bootCount", 0) + 1;
  prefs.putUInt("bootCount", bootCount);
  prefs.end();
  Serial.printf("[nvs] boot count (should increment across resets if NVS is healthy): %u\n",
                bootCount);
}

static void readButtons() {
  static bool lastA = true, lastB = true, lastC = true;
  bool a = digitalRead(PIN_BUTTON_A);
  bool b = digitalRead(PIN_BUTTON_B);
  bool c = digitalRead(PIN_BUTTON_C);
  if (a != lastA) { Serial.printf("[btn] A %s\n", a ? "released" : "pressed"); lastA = a; }
  if (b != lastB) { Serial.printf("[btn] B %s\n", b ? "released" : "pressed"); lastB = b; }
  if (c != lastC) { Serial.printf("[btn] C %s\n", c ? "released" : "pressed"); lastC = c; }
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) {
    // wait for USB CDC, but don't hang forever if nothing's attached
  }

  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_BUTTON_A, INPUT_PULLUP);
  pinMode(PIN_BUTTON_B, INPUT_PULLUP);
  pinMode(PIN_BUTTON_C, INPUT_PULLUP);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Serial1.begin(115200, SERIAL_8N1, PIN_UART1_RX, PIN_UART1_TX);

  Serial.println(F("=== RTK-Feather HUZZAH32 bring-up (Phase 1) ==="));
#if FAKE_NMEA_SOURCE
  Serial.println(F("[cfg] FAKE_NMEA_SOURCE=1 -- generating canned NMEA on Serial1 (no F9P expected)"));
#else
  Serial.println(F("[cfg] FAKE_NMEA_SOURCE=0 -- expecting real NMEA from the F9P on Serial1 RX"));
#endif

  i2cScan();
  nvsRoundTrip();

  // Display test (checklists.md: "run the matching Adafruit example; confirm buttons
  // register") -- shows live button states so the whole checklist item is this one sketch.
  oledUp = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  Serial.printf("[oled] SSD1306 128x32 %s\n", oledUp ? "up" : "NOT FOUND");
}

void loop() {
  static uint32_t lastBlink = 0;
  uint32_t now = millis();
  if (now - lastBlink >= 500) {
    lastBlink = now;
    digitalWrite(PIN_LED_RED, !digitalRead(PIN_LED_RED));
  }

  readButtons();

  // OLED test screen at 5 Hz: title + live button states (pressed = inverse video).
  static uint32_t lastOled = 0;
  if (oledUp && now - lastOled >= 200) {
    lastOled = now;
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextWrap(false);
    oled.setCursor(0, 0);
    oled.print(F("RTK-Feather bring-up"));
    oled.setCursor(0, 8);
    oled.print(F("press A / B / C:"));
    const struct { const char* name; int pin; } btns[] = {
        {"A", PIN_BUTTON_A}, {"B", PIN_BUTTON_B}, {"C", PIN_BUTTON_C}};
    for (int i = 0; i < 3; i++) {
      int x = 20 + i * 36;
      bool pressed = digitalRead(btns[i].pin) == LOW;
      if (pressed) {
        oled.fillRect(x - 3, 19, 12, 11, SSD1306_WHITE);
        oled.setTextColor(SSD1306_BLACK);
      }
      oled.setCursor(x, 21);
      oled.print(btns[i].name);
      oled.setTextColor(SSD1306_WHITE);
    }
    oled.display();
  }

#if FAKE_NMEA_SOURCE
  fakeNmea.tick();  // writes to Serial1 (consumers) and Serial (bench visibility) itself
#else
  while (Serial1.available()) {
    Serial.write(Serial1.read());  // pass real F9P NMEA through to the USB console
  }
#endif
}
