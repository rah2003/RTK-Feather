# Open Clarifying Questions (Project Brief §10)

The brief gates all firmware on these. Items 1–5 must not be guessed.
Status legend: ❓ needs your answer · 📄 resolved from vendor docs (confirm) ·
✅ answered.

## 1. ❓ Prior Metro-project codebase — found; import mechanism still open

**Resolved (2026-07-11): the Metro codebase exists**, per the owner's own
check rule ("if it's not in the SW Maps GitHub, it doesn't exist"). It's at
[`rah2003/SWMaps-propertylines`](https://github.com/rah2003/SWMaps-propertylines),
branch `claude/esp32-rtk-bridge-firmware-yokgan` (confirmed via the
unauthenticated GitHub API — no auth prompt needed, repo is public).
Session `session_01W7J4gwxntpHWWW3kmP5T17` itself remains inaccessible from
here (as a separate prior session on this same repo also found), but that no
longer matters — the *code* is reachable directly.

**What's there:** Phases 1–3 complete (bring-up, Rover: NTRIP + BLE NUS +
RAWX/CSV logging, Base: survey-in/fixed + RTCM3-on-UART2 config-only), never
run on real hardware. `docs/00-questions-and-assumptions.md` in that repo
shows the owner answered its own Section-10-equivalent questions on
2026-07-08 — including NTRIP caster (`acorn-gnss.net:2101`, mounts
`VRS_SouthCentral_RTCM3`/`MS_RTCM3`; credentials themselves are **not** in
that repo either), WiFi (iPhone hotspot), elevation mask (12°),
constellations (GPS+GLO+GAL+BDS, SBAS/QZSS off). `firmware/src/rtkbridge/`
has directly reusable pieces: `ringbuf.h` (lock-free SPSC byte ring, cross-
core atomics), `settings.h` (NVS/SD-config schema), `gnss_config.h` (takes
an abstract `DevUBLOXGNSS&` — the same transport already works for I2C or
UART, which lines up with this repo's Topology A/B question), `ntrip_client`,
`shared.h` (ring-buffer ownership + status-snapshot pattern).

**Decided (2026-07-11):** (a) import mechanism = **ported/adapted copy** into
`firmware/{common,huzzah32/rover}/`, not a submodule — Topology B and the
two-MCU split diverge enough from Metro's single-MCU architecture that a
live dependency would fight the adaptation constantly. (b) **inherit Metro's
answered defaults wholesale**: elevation mask 12°, GPS+GLO+GAL+BDS
constellations, config via NVS + serial menu (SD `/config.txt` doesn't port
directly — the HUZZAH32 has no onboard SD, only the M0 does, and the M0 isn't
the settings owner; see `firmware/huzzah32/rover/settings.h`'s header
comment). Real credentials still come from you directly (serial menu, or a
gitignored `firmware/huzzah32/rover/secrets.h` for bench convenience — never
committed, see `secrets.example.h`). (c) **same caster**:
`acorn-gnss.net:2101`, mount `VRS_SouthCentral_RTCM3`/`MS_RTCM3`.

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

## 4. ✅ BLE to SW Maps in v1? — WiFi-only for v1

Decided 2026-07-11: BLE deferred to Phase 4. `FEATURE_BLE=0` in
`[env:huzzah32-rover]`; `firmware/huzzah32/rover/ble_bridge.cpp` is a stub
that `#error`s if someone flips the flag without also adding the NimBLE
dependency and porting Metro's `ble_bridge.cpp` — no silent half-build.

## 5. ✅ NTRIP caster / WiFi details — same as Metro

Decided 2026-07-11: `acorn-gnss.net:2101`, mounts
`VRS_SouthCentral_RTCM3`/`MS_RTCM3`, WiFi via iPhone personal hotspot.
Baked into `firmware/huzzah32/rover/settings.h` defaults (host/port/mount
only — real username/password are never defaulted in code; see Q1 above).

## 6. ✅ Base mode required for this interim device? — Rover-only for v1

Decided 2026-07-11: Rover mode first. `FEATURE_BASE=0` in
`[env:huzzah32-rover]`; `gnss_config.cpp` always forces `CFG-TMODE-MODE=0`
(rover). Base mode is Phase 3.

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
