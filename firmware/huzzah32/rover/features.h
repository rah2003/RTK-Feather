#pragma once

// Compile-time feature flags, normally set from platformio.ini ([env:huzzah32-rover]
// build_flags). Defaults here make the code buildable standalone.
//
// Unlike the Metro project (rah2003/SWMaps-propertylines), there's no GNSS_VIA_I2C choice
// here: Topology B (docs/hardware/topology.md) forecloses I2C entirely -- ArduSimple's Lite
// documentation exposes no SDA/SCL pads -- so this build only ever talks F9P UART1.

#ifndef FEATURE_NTRIP
#define FEATURE_NTRIP 1  // WiFi + NTRIP client + RTCM injection
#endif

#ifndef FEATURE_BLE
#define FEATURE_BLE 0  // NimBLE NUS bridge to SW Maps -- OFF for v1 (WiFi-only). Flip to 1
#endif                 // and add h2zero/NimBLE-Arduino to lib_deps for the Phase 4 build.

#ifndef FEATURE_BASE
#define FEATURE_BASE 0  // Base-mode F9P config (survey-in/fixed, RTCM3 on UART2) -- OFF for
#endif                  // v1 (Rover-only per project decision). Phase 3 item.
