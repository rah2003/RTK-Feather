# Test Plan — Interim RTK Field Device

Sequenced so that everything testable *today* (boards not all on the bench,
GNSS possibly not attached) comes first. Each stage gates the next.

## Stage 0 — No hardware at all (PC only)

| Test | How | Pass criteria |
|---|---|---|
| NTRIP caster reachability | `curl` / python script against the caster with the field credentials, request the mountpoint, send a fixed GGA | ICY/HTTP 200 + RTCM3 bytes flowing (0xD3 sync visible) |
| RTCM sanity | Pipe 60 s of caster output through RTKLIB `str2str` or a small parser | Valid RTCM3 frames, plausible message set (1005/1074/1084/1094/1124 or MSM equivalents) |
| convbin dry run | Convert any sample `.ubx` (e.g. from PaulZC's repo test data) | RINEX obs/nav files produced |

## Stage 1 — HUZZAH32 alone (testable today, no GNSS)

Uses the **fake NMEA generator**: a firmware build flag substitutes a canned
NMEA source (GGA/RMC/GSV at 1 Hz with a fixed Anchorage-area position) for
UART1 input.

1. **Bring-up sketch**: blink red LED, I2C scan finds OLED at 0x3C, buttons
   A/B/C read correctly (note Button A pullup caveat — see
   `hardware/checklists.md`), NVS write/read survives reset.
2. **WiFi + NTRIP soak**: connect to caster with fake GGA upstream every
   ~10 s; run ≥ 2 h. Pass: no reboot, reconnect logic recovers from a
   forced AP-off event within backoff schedule, correction-age counter
   resets on each RTCM frame.
3. **BLE NUS bridge** (if enabled in v1): SW Maps on iPhone receives the
   fake NMEA stream; MTU ≥ 185 negotiated; sentence boundaries intact.
4. **Coexistence stress**: BLE streaming + NTRIP soak simultaneously; NTRIP
   byte gaps must not exceed those measured in test 2. BLE is allowed to
   degrade; NTRIP is not.

## Stage 2 — M0 Adalogger alone (testable today, no GNSS)

Uses a **recorded UBX stream replayed from a PC** over USB-serial (or a USB-UART
dongle into the M0's GNSS-input UART at the field baud rate).

1. **Bring-up sketch**: green/red LEDs, SD card init (CS = D4), SdFat bench
   write test ≥ 4 MB, status-link UART loopback.
2. **UBX frame extraction**: replay a mixed NMEA+UBX capture at 115200 (and
   230400 if adopted); logged `.ubx` must contain byte-identical
   RAWX/SFRBX frames vs. the source (compare with a PC-side script;
   checksum-validate every frame).
3. **Burst/latency margin**: replay at worst-case density (see
   `hardware/topology.md` link budget) while running SdFat with an
   artificially slow card; ring-buffer high-water mark must stay < 75 %.
4. **Safe shutdown**: stop command mid-write → file closes, FAT consistent
   (PC fsck / chkdsk clean), no truncated final frame.

## Stage 3 — F9P bench config (Lite + USB only)

Run the full `hardware/ucenter-config.md` checklist over the Lite's USB;
verify saved-to-flash config survives power cycle; confirm UART1 output at
the chosen baud on the Pixhawk connector with a 3.3 V USB-UART dongle
**before** any Feather is connected.

## Stage 4 — Integration (all boards on the bench)

Follow `hardware/power.md` smoke-test order strictly.

1. HUZZAH32 ↔ F9P UART1: NMEA visible, RTCM injection produces RTK FLOAT →
   FIX (with antenna + caster), GGA upstream accepted.
2. Add M0 tap / I2C (per selected topology): logging runs concurrently with
   corrections; confirm no interference (fix retention unchanged over 1 h).
3. Status link: M0 logging state appears on OLED page 3; time-synced file
   names match GNSS time.
4. Button UX walk-through per `field-guide.md`.
5. **Field dry run**: ≥ 30 min outdoor session; submit logged RAWX to
   CSRS-PPP; sanity-check against the RTK positions.

## Regression checklist (run before any field day)

- NTRIP reconnect after AP power-cycle
- Correction-age alarm (inverse-video flash) at > 10 s
- SD full / card-removed handling
- Safe-shutdown from both buttons-held gesture and status-link command
