# Logging Topology Decision (A vs B) and UART1 Link Budget

## The decision

The M0 Adalogger must receive RAWX/SFRBX from the ZED-F9P without touching
UART2/XBee (reserved for a future radio). Two candidate topologies:

- **Topology A — I2C**: M0 polls RAWX/SFRBX over the F9P's I2C (DDC) port
  (PaulZC `RAWX_Logger_F9P_I2C` pattern, SparkFun u-blox GNSS v3 file-buffer
  API). UART1 belongs exclusively to the HUZZAH32. Cleanest: one MCU per
  F9P port.
- **Topology B — shared-listener UART tap**: F9P UART1 TX fans out in
  parallel to the HUZZAH32 RX **and** the M0 RX (one driver, two listeners —
  electrically sound at 3.3 V CMOS). F9P UART1 RX is driven **only** by the
  HUZZAH32 TX. The M0 passively extracts UBX frames from the mixed
  NMEA+UBX stream — which is PaulZC's original `RAWX_Logger_F9P` design on
  its original board.

**Verification result: Topology B selected.** ArduSimple's Lite
documentation (hookup guide, datasheet, product page) exposes **no I2C
pads** — the only documented special-function pins are TIMEPULSE, RTK_STAT,
and GEOFENCE
([hookup guide](https://www.ardusimple.com/simplertk2blite-hookup-guide/)),
and in an ArduSimple support thread the vendor's own workaround for pad
access was soldering to an LED — there is no pad field to speak of.
Additionally, the Lite's "USB" is an FTDI adapter onto **UART1** via the
bottom XBee header (no native F9P USB), so UART1 is genuinely the only
data door for the Feathers. A physical photo-check of the board is still
requested (`QUESTIONS.md` Q3) since the doc sweep ran through search
extraction. Full wiring in `wiring.md`.

*(Repo correction vs. the project brief: PaulZC's UART and I2C logger
sketches both live in
[PaulZC/F9P_RAWX_Logger](https://github.com/PaulZC/F9P_RAWX_Logger) —
`Arduino/RAWX_Logger_F9P/` and `Arduino/RAWX_Logger_F9P_I2C/` — not in two
separate repos; a July-2024 refresh of the I2C variant lives in
[PaulZC/ZED-F9P_FeatherWing_USB](https://github.com/PaulZC/ZED-F9P_FeatherWing_USB).)*

### Tap discipline (Topology B) — read twice

- Exactly **one driver per line**. The M0's TX must **never** be wired to
  the F9P UART1 RX line — that line already has the HUZZAH32 as its driver.
  Leave the M0-side TX pin unconnected (and configured as input in
  firmware for defense in depth).
- The tap is TX-only into the M0: two listener RX inputs on one F9P TX
  driver. Two 3.3 V CMOS inputs are a trivial load; no buffer needed at
  these lengths (< 20 cm).
- Never connect u-center via a USB-UART dongle to UART1 pins while the
  Feathers are wired (second driver!). u-center goes through the Lite's own
  USB connector only.

## UART1 link budget (why 115200 works, why 230400 is the headroom option)

Traffic on UART1, F9P → HUZZAH32 (+ M0 tap in Topology B), per 1 Hz epoch:

**UBX-RXM-RAWX** — frame = 6 (header) + payload + 2 (checksum);
payload = 16 + 32 × numMeas. Worst case full multi-constellation,
dual-frequency tracking ≈ 72 measurements (≈ 36 SVs × 2 signals):

    16 + 32×72 = 2320 B payload → 2328 B frame ≈ 2.3 KiB per epoch

**UBX-RXM-SFRBX** — one message per decoded broadcast subframe;
payload = 8 + 4 × numWords (≈ 10 words typical → ≈ 56 B/frame). Bursty;
budget a worst case of ~30 frames/s ≈ 1.7 KiB/s sustained after cold start.

**UBX-TIM-TM2** (optional) — 28 B payload → 36 B/epoch. Negligible.

**NMEA** (GGA, RMC, GSA, GST, VTG + GSV for 4 constellations):
GSV dominates at ~60–70 B per sentence × ~12–16 sentences ≈ 1.0 KiB, plus
~0.4 KiB for the rest ≈ 1.5 KiB/s. (Trim GSV rate to 1-in-5 epochs in
config if margin is ever tight.)

**Totals (worst case, 1 Hz):**

| Stream | B/s |
|---|---|
| RAWX | ~2,330 |
| SFRBX | ~1,700 |
| NMEA | ~1,500 |
| TIM-TM2 | ~36 |
| **Sum** | **~5,600 B/s** |

Capacity: 115200 baud, 8N1 → 11,520 B/s ⇒ **~49 % worst-case utilization**.
230400 → 23,040 B/s ⇒ ~24 %.

Downstream (HUZZAH32 → F9P) RTCM3 corrections are ~0.5–1 KiB/s — irrelevant
on the opposite wire.

**Recommendation:** start at **115200** (2× margin, kinder to the M0's ISR
budget and to long jumper runs); switch to **230400** only if we later add
messages or raise the nav rate. Both MCUs and the F9P support either.

## M0 buffering math (32 KiB RAM discipline)

Input rate ≤ 11,520 B/s (at 115200; the M0 sees *everything* on the tap in
Topology B, and filters to UBX). SD cards stall: budget a worst-case single
write latency of 250 ms (cheap card, FAT housekeeping).

    Required absorption = 11,520 B/s × 0.25 s ≈ 2.9 KiB

**Two-stage buffering (revised after code review, 2026-07-11).** The
original single-number analysis ("16 KiB ring absorbs the stall") missed
that the ring only sees bytes that already survived the *first* stage: while
`logFile.write()` is stalled, the superloop is blocked and the **SERCOM UART
RX interrupt buffer is the only absorber** — and the Adafruit core's default
is just 350 B ≈ 30 ms. The revised budget:

1. **Stage 1 — UART RX ISR buffer: 1024 B ≈ 89 ms** of full-tap stream
   (`-DSERIAL_BUFFER_SIZE=1024` in `platformio.ini`). This must ride out the
   longest single `write()` stall. It can't be made bigger cheaply: the
   define is global and each `Uart` object carries an RX *and* TX ring at
   this size, ×3 instances (Serial1, Serial2, core-hidden Serial5) — 2048
   measured at 98 % RAM. Mitigations that shrink the stall instead: files
   are **pre-allocated contiguous** (32 MiB, trimmed at close) so writes
   skip FAT housekeeping, and the drain loop writes **at most one 512 B
   chunk per loop pass** so the UART is serviced between chunks. The Stage 2
   slow-card bench test (test-plan.md) is the arbiter of whether 89 ms
   covers the real card's latency tail — watch `ckErr` (UART overflow shows
   up as checksum errors) and `framesDropped`.
2. **Stage 2 — 12 KiB UBX ring** (`UbxExtractor`, down from the original
   16 KiB to pay for stage 1): absorbs ≈ 1.07 s of *extracted* backlog,
   ~4.3× the budgeted 250 ms stall. Frames that don't fit are dropped
   whole and counted (`framesDropped`) — never split.

Static allocation only; no `String`. Writes to SD in 512 B-aligned chunks.

**Measured (2026-07-11, post-review build, `arm-none-eabi-nm` on the ELF):**
static RAM = 26,088 B of 32,768 (79.6 %), leaving **~6.7 KiB** for stack +
heap. The original "> 8 KiB free" estimate had missed the extractor's 3 KiB
frame-assembly buffer, ~1.4 KiB of native-USB CDC buffers, and the core's
hidden `Serial5` (which also pays the SERIAL_BUFFER_SIZE tax — dead weight,
but unavoidable without patching the core). SdFat is only ~1.2 KiB (built
FAT-only via `-DSDFAT_FILE_TYPE=1`; exFAT support costs flash, not RAM).
6.7 KiB is adequate for this no-malloc superloop; treat it as the budget
floor — any new buffer must come out of the 12 KiB ring, not stack headroom.

## OLED placement

The OLED FeatherWing rides the **HUZZAH32**:

- The HUZZAH32 natively owns everything the display shows on 3 of 4 pages
  (WiFi, caster state, correction age, fix type from parsed NMEA, BLE).
  Logging status arrives over the status link anyway.
- Its I2C bus is free in both topologies and stays short (stacked Wing).
- Buttons must live where mode/shutdown decisions are made (the ESP32 owns
  mode + NVS persistence).

Honest alternative — OLED on the M0: saves nothing (the M0 would then need
all NTRIP/WiFi state shipped *to* it over the status link, i.e. more
traffic, inverted), and costs display buffer + driver RAM (1 KiB
framebuffer for 128×64 + Adafruit_GFX overhead) out of the 32 KiB that the
SD ring buffer needs. In Topology A it would also put the OLED and the F9P
on the same M0 I2C bus — workable (0x3C vs 0x42) but adds cross-board bus
length and pullup questions for zero benefit. **Rejected.**

## Inter-board status link

Spare ESP32 UART ↔ M0 SERCOM UART, 3 wires (TX, RX, GND), 38400 baud,
line-based ASCII with checksum (`$STA,logging,file,bytes*CS`-style; exact
protocol defined with the firmware). ESP32 → M0: time sync, log start/stop,
safe shutdown. M0 → ESP32: logging state, filename, bytes written, SD free.
Exact pins in `wiring.md`.
