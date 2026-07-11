// Cross-task shared state and the ring buffer that connects the cores. Adapted from the
// Metro project's shared.h (rah2003/SWMaps-propertylines): that version also carries
// nmeaRing (BLE consumer) and sdRing + battery fields (Metro's onboard microSD + MAX17048
// fuel gauge, both physically absent on the HUZZAH32). Neither applies here:
//   - no nmeaRing: BLE is compiled out for v1 (FEATURE_BLE=0), so there is no consumer.
//   - no sdRing/battery: the HUZZAH32 has no onboard SD or fuel gauge -- the M0 owns
//     logging entirely, and reports its own state back over the status link (M0Status
//     below), not through a shared ring buffer on this MCU.
//
// Ownership map (docs/02-design.md-equivalent for this project -- see gnss_task.cpp /
// ntrip_client.cpp headers for the per-file version):
//   rtcmRing : producer ntripTask (core 0) -> consumer gnssTask (core 1)
// GnssStatus / M0Status are snapshot structs guarded by a mutex: the owning task writes,
// everyone else reads through the getters. The SparkFun GNSS library object itself is
// touched ONLY by gnssTask (Metro's "one caller" rule -- the library isn't thread-safe).
#pragma once
#include <Arduino.h>

#include "../../common/ringbuf.h"

struct GnssStatus {
  bool f9pDetected = false;
  uint8_t fixType = 0;   // NAV-PVT fixType: 0 none, 2 2D, 3 3D
  uint8_t carrSoln = 0;  // 0 none, 1 float, 2 fixed
  uint8_t numSV = 0;
  double latDeg = 0, lonDeg = 0;
  double hMslM = 0;
  uint32_t hAccMm = 0, vAccMm = 0;
  float pdop = 0;
  bool timeValid = false;
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
  char lastGga[120] = {0};  // most recent GGA sentence (for NTRIP upstream)
  uint32_t lastGgaMs = 0;
};

struct LinkStatus {
  bool wifiUp = false;
  bool ntripConnected = false;
  uint32_t lastRtcmMs = 0;  // last time RTCM bytes arrived from the caster
  uint32_t rtcmBytes = 0;
  int8_t wifiRssi = 0;  // dBm, published by ntripTask (the WiFi owner) for the OLED; 0 = unknown
};

// M0's self-reported state, received over the status link (common/status_link.h MSTA
// message). Written by status_link_task only. linkUp is DERIVED at read time in
// statusGetM0() from lastMsgMs (stale after 5 s of silence) -- a latched flag would show
// "link up" forever after the M0 died mid-session.
struct M0Status {
  bool linkUp = false;
  bool logging = false;
  char fileName[40] = {0};  // sized to match the M0's filename buffer (status_link.h)
  uint32_t bytesWritten = 0;
  uint32_t sdFreeKB = 0;
  uint32_t lastMsgMs = 0;
};

extern RingBuf<8192> rtcmRing;  // caster -> F9P; ~7 s of a heavy 1.2 kB/s VRS stream

void statusInit();
void statusSetGnss(const GnssStatus& s);  // gnssTask only
GnssStatus statusGetGnss();
void statusSetM0(const M0Status& s);  // status_link_task only
M0Status statusGetM0();

extern LinkStatus g_link;  // simple flags, written by ntripTask only

// Correction age in ms (UINT32_MAX if never received).
uint32_t correctionAgeMs();
