#pragma once

// Pin map for Phase 1 M0 Adalogger bring-up -- from docs/hardware/wiring.md and
// docs/hardware/checklists.md; see those files for vendor-doc sources.

constexpr int PIN_LED_GREEN = 8;
constexpr int PIN_LED_RED = 13;

constexpr int PIN_SD_CS = 4;
constexpr int PIN_SD_CD = 7;  // card-detect, INPUT_PULLUP: HIGH = card present, LOW = none

// Serial1 (D0/D1, SERCOM0) is the GNSS UART tap in the final wiring (D1 TX must stay
// unconnected -- wiring.md NEVER-connect #1: the F9P UART1 RX line's only driver is the
// HUZZAH32). This bring-up sketch does NOT use Serial1: the UBX replay harness reads from
// the native USB serial (`Serial`) instead, per test-plan.md Stage 2's "replayed from a PC
// over USB-serial" option, so it needs no extra hardware to run today.
//
// Serial2 (status link, D10 TX / D11 RX, SERCOM1) is brought up separately -- see
// serial2_link.h for the exact SERCOM recipe (checklists.md: the classic SAMD21 footgun).
