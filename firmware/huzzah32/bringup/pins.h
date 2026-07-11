#pragma once

// Pin map for Phase 1 HUZZAH32 bring-up. Values are not re-derived here -- they're the
// already vendor-doc-cited assignments from docs/hardware/wiring.md and
// docs/hardware/checklists.md; see those files for sources.

constexpr int PIN_LED_RED = 13;

// OLED FeatherWing I2C. HUZZAH32 has no onboard I2C pullups -- the Wing supplies them
// (learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts, cited in wiring.md).
constexpr int PIN_I2C_SDA = 23;
constexpr int PIN_I2C_SCL = 22;

// OLED Wing buttons. A=GPIO15 is the MTDO strapping pin -- checklists.md: don't hold
// through a reset. B=GPIO32 has a 100k onboard pullup (INPUT_PULLUP is redundant but
// harmless). All three read LOW when pressed.
constexpr int PIN_BUTTON_A = 15;
constexpr int PIN_BUTTON_B = 32;
constexpr int PIN_BUTTON_C = 14;

// Serial1 = F9P UART1 tap. GPIO17 TX -> F9P UART1 RX (RTCM3 + config out), GPIO16 RX <- F9P
// UART1 TX (NMEA + UBX in). Default feather_esp32 variant pins (wiring.md).
constexpr int PIN_UART1_RX = 16;
constexpr int PIN_UART1_TX = 17;

// Serial2 (status link to the M0, GPIO27 RX / GPIO33 TX, wiring.md wires 5/6) is Stage 4
// integration territory -- not used by this Phase 1 bring-up sketch.
