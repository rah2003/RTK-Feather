#include "ble_bridge.h"

#include "features.h"

#if FEATURE_BLE
#error \
    "FEATURE_BLE=1 needs h2zero/NimBLE-Arduino in lib_deps and Metro's ble_bridge.cpp " \
    "(rah2003/SWMaps-propertylines) ported in -- not done for this v1 build. Either add " \
    "that work or leave FEATURE_BLE=0."
#else
void bleTaskStart() {}  // WiFi-only v1 -- see ble_bridge.h
#endif
