#pragma once
#include <Arduino.h>

// Canned NMEA source standing in for a real F9P on Serial1 when none is on the bench
// (docs/test-plan.md Stage 1: "a firmware build flag substitutes a canned NMEA source for
// UART1 input"). Call tick() once per loop() with no rate-limiting of your own -- it
// self-paces to 1 Hz. Emits GGA + RMC every second and one GSV set every 5th second, to
// both Serial1 (what downstream NTRIP/BLE code will consume) and Serial (USB bench
// visibility), checksummed correctly at runtime. Fixed at a canned Anchorage-area position
// (61.2181 N, 149.9003 W), matching the 61 N elevation-mask reference used elsewhere in the
// project docs (hardware/topology.md, project brief §9).
class FakeNmeaSource {
 public:
  // Returns true if a tick fired (and sentences were written) this call.
  bool tick();

 private:
  uint32_t _lastTickMs = 0;
  uint32_t _epoch = 0;
  uint8_t _hour = 14, _minute = 0, _second = 0; // fake UTC clock, arbitrary start
};
