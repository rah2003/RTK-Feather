# RTK-Feather — Interim RTK Field Device

Interim RTK rover/base built from on-hand parts while the primary build
(Adafruit Metro ESP32-S3 + simpleRTK2B Basic) waits on hardware. Shared
firmware modules are structured as a common library so this build is a head
start on the Metro build, not a fork.

## Hardware

| Role | Board |
|---|---|
| Radio/bridge MCU | Adafruit HUZZAH32 Feather (ESP32-WROOM) — WiFi/NTRIP, BLE NUS, OLED UI |
| Logger MCU | Adafruit Feather M0 Adalogger (SAMD21) — RAWX logging to microSD |
| GNSS | ArduSimple simpleRTK2B Lite (u-blox ZED-F9P) |
| Display | Adafruit OLED FeatherWing (variant TBC: 128×32 SSD1306 or 128×64 SH1107) |
| Phone | iPhone + SW Maps over BLE NUS (secondary priority) |

Hard constraints:

1. **F9P UART2 / XBee socket is reserved** for a future plug-in radio. No
   firmware function may depend on it (Base mode may *configure* RTCM3
   output on UART2 — config only, no wiring).
2. **F9P UART1 is the ESP32's working port** (RTCM3 in; NMEA + UBX out),
   reached via the Pixhawk JST-GH connector.
3. **WiFi/NTRIP reliability outranks BLE.** If anything degrades, it is BLE.

## Repository layout

```
docs/
  QUESTIONS.md                Open clarifying questions (Section 10) + current answers
  hardware/
    topology.md               Topology A vs B decision, link budget math
    wiring.md                 Full-system wiring diagrams (all boards + OLED)
    checklists.md             Per-board bring-up checklists
    ucenter-config.md         F9P pre-configuration over USB (u-center)
    power.md                  Power tree, forbidden combinations, smoke test
  field-guide.md              One-page field reference (buttons, pages, PPP workflow)
  test-plan.md                Bench test sequence, incl. no-GNSS-attached tests
firmware/
  common/                     Portable code shared across environments: ringbuf.h (ESP32
                               cross-core ring buffer, ported from Metro), status_link.h/.cpp
                               (HUZZAH32<->M0 protocol), ubx_extractor.h/.cpp (passive UBX
                               frame parser, used by both M0 build variants)
  huzzah32/
    bringup/                  Phase 1 bench sketch: LED/I2C-scan/button/NVS + fake NMEA
    rover/                    Phase 2 Rover firmware (NTRIP -> RTCM3 -> F9P UART1, status
                               link, buttons, OLED stub) -- ported/adapted from the Metro
                               codebase (rah2003/SWMaps-propertylines); see file headers for
                               per-file adaptation notes. secrets.example.h -> copy to
                               secrets.h (gitignored) for bench credential convenience.
  adalogger_m0/
    pins.h, serial2_link.*    Shared between both M0 build variants
    bringup/                  Phase 1 bench sketch: LED/SD-write + status-link loopback +
                               USB-replayed UBX harness
    rover/                    Phase 2 Rover firmware: passive UART1 tap -> .ubx on SD,
                               GNSS-time-derived filenames, status-link command handling
platformio.ini                Four environments: {huzzah32,adalogger_m0}-{bringup,rover}
```

## Status

- [x] Hardware verification pack (docs/) — **verify wiring against these
      documents and current vendor docs before powering anything**
- [~] Section 10 clarifying questions — caster/WiFi (Q1, Q5-equivalent),
      Topology B (Q3), BLE-off-for-v1 (Q4), Rover-first (Q3-equivalent) are
      answered (see `docs/QUESTIONS.md`); Q2 (OLED variant), Q6-Q10, A-C
      remain open
- [x] Phase 1: per-board bring-up sketches (`firmware/{huzzah32,adalogger_m0}/bringup/`)
      — scaffolding in, not yet run on real hardware
- [~] Phase 2: Rover firmware written (`firmware/{huzzah32,adalogger_m0}/rover/`)
      — **compiles-on-paper only; not yet built or run on real hardware** (no
      toolchain in this environment). NTRIP -> RTCM3 -> F9P UART1, GNSS
      config (Topology B, UART1-only, 115200), status link (time sync + log
      start/stop + safe shutdown), passive UBX tap -> time-named .ubx on SD,
      button gestures (matches the already-drafted `field-guide.md` scheme).
      BLE compiled out (`FEATURE_BLE=0`, no NimBLE dependency). OLED
      rendering is a stub (prints status to serial) pending Q2. Fix-quality
      CSV logging (brief §9 suggestion) is NOT implemented — flagged, not
      silently built. **Needs a real `pio run` + hardware bring-up before
      any of this is trustworthy — see "What's unverified" below.**
- [ ] Phase 3: Base mode
- [ ] Phase 4: polish (OLED menus, BLE toggle)

## Reference: the Metro codebase

The primary build (Metro ESP32-S3 + simpleRTK2B) lives at
[`rah2003/SWMaps-propertylines`](https://github.com/rah2003/SWMaps-propertylines),
branch `claude/esp32-rtk-bridge-firmware-yokgan`, with Phases 1–3 already
built. Its answered questions (caster `acorn-gnss.net:2101`, elevation mask
12°, constellations GPS+GLO+GAL+BDS, WiFi via iPhone hotspot) are inherited
wholesale as this build's defaults (`firmware/huzzah32/rover/settings.h`).
Import mechanism: **ported/adapted copy**, not a submodule (decided —
Topology B and the two-MCU split diverge enough from Metro's single-MCU
architecture that a live dependency would fight the adaptation at every
turn). Each ported file's header comment says what changed and why.

## What's unverified

Nothing in `firmware/*/rover/` has been compiled or run. Known risks worth
checking first, in rough priority order:
1. SparkFun u-blox GNSS v3 constant names (`UBLOX_CFG_*`) in `gnss_config.cpp`
   are transcribed from the Metro project's code and cross-checked against
   `docs/hardware/ucenter-config.md`'s independently researched key IDs, but
   neither codebase has actually been built — a typo'd constant name would
   only surface at compile time.
2. `platformio.ini`'s `build_src_filter` patterns are hand-written against
   the directory layout above, not tested against a real PlatformIO run.
3. The button/status-link/logging state machine has no hardware-in-the-loop
   test yet — Phase 1's bring-up sketches exercise the pieces individually
   (fake NMEA, USB-replayed UBX) but not the full Rover firmware end-to-end.
