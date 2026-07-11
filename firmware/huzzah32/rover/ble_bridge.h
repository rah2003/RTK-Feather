#pragma once

// Core 0. Nordic UART Service bridge to SW Maps -- compiled OUT for v1 (FEATURE_BLE=0,
// WiFi-only per project decision; docs/QUESTIONS.md Q4). No NimBLE dependency in
// platformio.ini for this build. Flip FEATURE_BLE to 1 in [env:huzzah32-rover] build_flags,
// add h2zero/NimBLE-Arduino to lib_deps, and port Metro's ble_bridge.cpp
// (rah2003/SWMaps-propertylines) as the starting point for the Phase 4 build.
void bleTaskStart();
