#pragma once

// Pin map for the HUZZAH32 Rover firmware (Phase 2), Topology B. Sources: already
// vendor-doc-cited in docs/hardware/wiring.md and docs/hardware/checklists.md.

constexpr int PIN_LED_RED = 13;

// OLED FeatherWing I2C. HUZZAH32 has no onboard I2C pullups -- the Wing supplies them
// (learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts, cited in wiring.md).
constexpr int PIN_I2C_SDA = 23;
constexpr int PIN_I2C_SCL = 22;

// OLED Wing buttons -- verified 2026-07-11 against Adafruit's own FeatherWing example
// (Adafruit_SSD1306/examples/OLED_featherwing/OLED_featherwing.ino: `#elif defined(ESP32)`
// block defines BUTTON_A 15 / BUTTON_B 32 / BUTTON_C 14; the "9/6/5" numbers seen in the
// guide are the same physical Wing positions as labeled on AVR/M0-class Feathers).
// A=GPIO15 is the MTDO strapping pin -- don't hold through a reset. B=GPIO32 has a 100k
// onboard pullup (INPUT_PULLUP is redundant but harmless). All three read LOW when pressed.
constexpr int PIN_BUTTON_A = 15;
constexpr int PIN_BUTTON_B = 32;
constexpr int PIN_BUTTON_C = 14;

// Serial1 = F9P UART1 tap (wiring.md wires 2/3). GPIO17 TX -> F9P UART1 RX (RTCM3 + config
// out), GPIO16 RX <- F9P UART1 TX (NMEA + UBX in -- this is also what the M0 taps in
// parallel; wire 3's "same wire, parallel tap" in wiring.md).
constexpr int PIN_UART1_RX = 16;
constexpr int PIN_UART1_TX = 17;

// Serial2 = status link to the M0 (wiring.md wires 5/6), remapped via the GPIO matrix:
// `Serial2.begin(38400, SERIAL_8N1, PIN_STATUS_RX, PIN_STATUS_TX)`.
constexpr int PIN_STATUS_RX = 27;  // <- M0 D10
constexpr int PIN_STATUS_TX = 33;  // -> M0 D11
