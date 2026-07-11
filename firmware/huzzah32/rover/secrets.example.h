#pragma once

// Copy this file to secrets.h (same directory) and fill in real values for bench
// convenience. secrets.h is gitignored -- see .gitignore at the repo root -- so it never
// gets committed. This is entirely optional: credentials can always be set at runtime via
// the serial menu instead (persisted to NVS), which is the only mechanism available once a
// device is in the field with no PC attached.
//
// settingsLoad() (settings.cpp) seeds these into Settings ONLY when the corresponding NVS
// field is still empty (a fresh device, or after an NVS erase), then immediately persists
// them to NVS -- so secrets.h only needs to exist for that first boot; after that the NVS
// copy is authoritative and secrets.h can even be deleted.
//
// Unlike the Metro project (rah2003/SWMaps-propertylines), this board has no onboard SD
// card, so the "/config.txt on SD" mechanism Metro uses doesn't have anywhere to live here
// -- this compiled-in-seed + serial-menu combination is this build's equivalent.

// #define RTKF_WIFI_SSID1  "your-hotspot-ssid"
// #define RTKF_WIFI_PASS1  "your-hotspot-password"

// Caster: acorn-gnss.net:2101, mount VRS_SouthCentral_RTCM3 or MS_RTCM3 (settings.cpp
// defaults) -- only the username/password are secret.
// #define RTKF_CASTER_USER "your-ntrip-username"
// #define RTKF_CASTER_PASS "your-ntrip-password"
