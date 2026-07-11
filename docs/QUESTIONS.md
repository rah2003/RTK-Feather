# Open Clarifying Questions (Project Brief §10)

The brief gates all firmware on these. Items 1–5 must not be guessed.
Status legend: ❓ needs your answer · 📄 resolved from vendor docs (confirm) ·
✅ answered.

## 1. ❓ Prior Metro-project codebase access

This repository (`rah2003/RTK-Feather`) is **empty** — no branches, no
commits — and the referenced Claude Code session
(`session_01W7J4gwxntpHWWW3kmP5T17`) is not accessible from this session:
sessions don't share filesystems, and no Metro repo is attached here.

**Needed:** where does the Metro project code/design live? Options:
(a) it's in another GitHub repo — name it and I'll request it be added to
this session; (b) it exists only in that session — resume that session and
push its work to a repo first; (c) it produced no code yet — then this repo
*becomes* the shared-library origin and the Metro build will consume it
later (my recommendation if (a)/(b) aren't quick). Also confirm:
import-as-library vs copy-modules-in.

## 2. ❓ OLED FeatherWing variant on hand

128×32 (SSD1306, product #2900) or 128×64 (SH1107, product #4650)?
Driver library, page layouts, and button behavior differ. A photo or the
product number on the silk/bag resolves it.

## 3. 📄 simpleRTK2B Lite I2C pads → Topology A vs B

**Resolved from docs: no I2C pads → Topology B (shared-listener UART
tap).** ArduSimple's hookup guide, datasheet, and product page document
only TIMEPULSE, RTK_STAT, and GEOFENCE as special-function pins, and the
Lite's "USB" turns out to be an FTDI bridge onto UART1 itself (details and
citations in `hardware/wiring.md` and `hardware/topology.md`).
**Please still photo-check the physical board** for any labeled SDA/SCL
pad — the doc sweep was done via search extraction and a board revision
could differ from the indexed docs.

## 4. ❓ BLE to SW Maps in v1?

Is BLE NUS → SW Maps wanted in this interim build's v1, or is WiFi-only
acceptable with BLE added in Phase 4? (Firmware will carry a compile-time
flag to build BLE fully out either way.)

## 5. ❓ NTRIP caster / WiFi details

Same caster, mountpoint, credentials, and WiFi source (phone hotspot?
dedicated hotspot?) as answered for the Metro project — or different here?
I don't have the Metro session's answers (see Q1), so please restate:
caster host:port, mountpoint, auth, NTRIP v1 or v2, and the field WiFi
plan.

## 6. ❓ Base mode required for this interim device?

Rover-only until the Metro build, or is Base (survey-in / fixed coords,
RTCM on UART2 config-only) genuinely needed here?

## 7. ❓ Field power plan

One USB battery bank powering everything (recommended in
`hardware/power.md`)? LiPos on either Feather? Bank model/port count if
known — the power tree assumes a 2-port bank.

## 8. ❓ Physical assembly

FeatherWing Doubler / Tripler / project board? Enclosure plans? This
affects the wiring diagram's connector recommendations (JST-GH pigtail
routing, SMA strain relief).

## 9. ❓ PlatformIO confirmed?

Two environments (`huzzah32`, `adalogger_m0`) in one `platformio.ini`, with
Arduino-IDE instructions as a fallback appendix. Confirm.

## 10. ❓ Does this device stay in service post-Metro?

E.g., as a dedicated base afterward — that would justify more polish on
Base mode (Q6) now.

---

### Additional questions from the hardware review

- **A.** Cable stock: do you have a JST-GH 6-pin pigtail (Pixhawk cable)
  for the Lite, or do we plan around soldered headers/pads?
- **B.** microSD card on hand: brand/size/speed class? (Affects the SD
  latency assumptions in `hardware/topology.md`.)
- **C.** GNSS antenna: which active antenna, and is a ground plane
  available for it?
