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
firmware/                     (created in Phase 1, after Section 10 answers)
  common/                     Shared library: NTRIP, NMEA/UBX parse, config
  huzzah32/                   ESP32 bridge firmware
  adalogger_m0/               SAMD21 RAWX logger firmware
platformio.ini                Two environments: huzzah32, adalogger_m0
```

## Status

- [x] Hardware verification pack (docs/) — **verify wiring against these
      documents and current vendor docs before powering anything**
- [ ] Section 10 clarifying questions answered (see `docs/QUESTIONS.md`)
- [ ] Phase 1: per-board bring-up sketches
- [ ] Phase 2: Rover complete (NTRIP → RTCM → F9P; RAWX logging; OLED status)
- [ ] Phase 3: Base mode
- [ ] Phase 4: polish (OLED menus, BLE toggle)

Firmware is intentionally absent until the questions in
`docs/QUESTIONS.md` are answered — the project brief gates code on them.
