# Field Reference Card (one page)

> Draft — button gestures below are the §9 *proposal* (pending approval in
> `QUESTIONS.md`); OLED layout finalizes once the Wing variant is known.

## Buttons (OLED FeatherWing, top-to-bottom A / B / C)

| Gesture | Action |
|---|---|
| **A short** | Cycle OLED pages 1→2→3→4 |
| **B long (≥1.5 s)** | Start/stop RAWX logging (on-screen confirm) |
| **A + C hold (≥3 s)** | Safe shutdown: flush + close SD files → "SAFE TO POWER OFF" |
| Wing reset button | ⚠ hard-resets the HUZZAH32 (whole board, not just display) |

Avoid holding **A** while the board resets (A = GPIO15, a boot-strap pin).

## OLED pages

1. **Position** — fix type (NO FIX / 3D / DGPS / FLOAT / **FIX**),
   lat/lon, sats used, HDOP
2. **NTRIP** — caster state, correction age (⚠ inverse-video flash when
   > 10 s), RTCM bytes/s, WiFi RSSI
3. **Logging** — file name, MB written, SD free, M0 link state
4. **BLE** — advertising/connected, client name, sentences/s

## LEDs

| LED | Meaning |
|---|---|
| HUZZAH32 red (GPIO13) | slow blink = WiFi connecting · solid = NTRIP connected · fast blink = correction age alarm |
| M0 green (D8) | heartbeat = logging · off = idle |
| M0 red (D13) | solid/blink pattern = SD error (see console) |

## SW Maps pairing (iPhone, BLE)

1. OLED page 4 → confirm BLE is enabled (menu toggle if off).
2. SW Maps → hamburger → **Bluetooth GNSS** → instrument model
   **Generic NMEA (Bluetooth LE)** → select the device
   (`RTK-Feather`) → Connect.
3. Verify fix type & accuracy appear in SW Maps; record features as usual.

## Power-up / power-down

- **Up:** bank on → both Feathers together → wait for OLED page 1 fix
  info. Logging auto-starts on GNSS time-fix (if configured) or via B-long.
- **Down:** A + C hold → wait for "SAFE TO POWER OFF" → bank off.
  (Pulling power mid-write risks the current `.ubx` file.)

## CSRS-PPP workflow (post-processing)

1. Take the microSD's `YYYYMMDD/r_HHMMSS.ubx` (rover) or `b_…ubx` (base).
2. Convert UBX → RINEX with RTKLIB `convbin` (verify flags with
   `convbin -h`; RTKLIB 2.4.3+):

   ```
   convbin -r ubx -v 3.04 -od -os -f 2 -o session.obs -n session.nav r_HHMMSS.ubx
   ```

3. Upload the `.obs` to CSRS-PPP (https://webapp.csrs-scrs.nrcan-rncan.gc.ca/geod/tools-outils/ppp.php),
   mode **Static** (base position refinement) or **Kinematic**, ITRF/NAD83
   per project datum.
4. Results arrive by email; apply the refined coordinates as Base-mode
   fixed position when Base phase is active.
