# Power Tree — ONE recommended scheme; everything else forbidden

## Recommended field scheme

```
USB battery bank (≥2 ports, ≥1 A/port)
 ├── Port 1 ── USB cable ──► HUZZAH32 micro-USB
 │                             ├── onboard AP2112 3V3 ──► ESP32 + OLED Wing (only)
 │                             └── "USB" header pin (5V VBUS)
 │                                   └──► Lite JST pin 1 (5V_IN) ──► F9P + active antenna
 └── Port 2 ── USB cable ──► M0 Adalogger micro-USB
                               └── onboard 3V3 ──► SAMD21 + microSD (only)

Common ground: JST pin 6 ↔ HUZZAH32 GND ↔ M0 GND (wired, not assumed)
```

Why this shape:

- **The Lite gets 5 V, not 3.3 V.** The HUZZAH32's AP2112 regulator is a
  500 mA-peak part with ~250 mA budgeted to the WROOM32's WiFi bursts
  ([Adafruit power management](https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/power-management)) —
  the F9P (~130 mA) + active antenna (≤75 mA) would consume the entire
  remaining headroom. Feeding the Lite from the **USB pin** (raw VBUS)
  bypasses the regulator entirely; the Lite regulates its own 3.3 V from
  the Pixhawk 5V_IN (4.5–5.5 V accepted,
  [hookup guide](https://www.ardusimple.com/simplertk2blite-hookup-guide/)).
- **Port-1 load ≈ 500 mA peak** (ESP32 WiFi bursts ~250 mA + OLED ~20 mA +
  Lite ~200 mA) — comfortably inside any 1 A bank port. Port-2 load
  ≈ 100–150 mA (SAMD21 + SD write bursts).
- **Bank auto-off caution:** many banks cut ports below ~50–100 mA. Port 1
  is safely above that; port 2 hovers near some banks' threshold — if the
  M0 browns out in the field, move both boards to a bank without
  low-current shutoff or power the M0 from a LiPo instead (it charges at
  100 mA whenever USB is present).

### LiPo policy

- Optional LiPos on either Feather act as UPS-style backup only (Feathers
  hot-swap to battery when USB drops). **But note:** the Lite is fed from
  VBUS — if bank power drops, GNSS dies even while the Feathers coast on
  LiPo. That's acceptable (M0 gets time to flush and close files — design
  the firmware to treat "GNSS stream stopped" as a close-file trigger) and
  is exactly why LiPo-on-M0 is the one genuinely useful battery here.
- Do **not** plan to run the field session from LiPos alone: nothing would
  power the Lite.

## Forbidden configurations

1. **Lite on any Feather 3V3 pin** — regulator headroom (above).
2. **Lite powered through the XBee-socket VCC path from a Feather** — same
   problem with extra steps; the top socket's VCC is also an *output*.
3. **u-center USB adapter attached while the JST pigtail is connected** —
   power is sanctioned by ArduSimple (multiple simultaneous sources: "no
   risk", [hookup guide](https://www.ardusimple.com/simplertk2blite-hookup-guide/)),
   but the adapter is a second **UART1 driver** — data contention. Bench
   rule: one or the other, never both (`wiring.md`, `ucenter-config.md`).
4. **Powering one Feather while the other is off but wired.** The
   unpowered board's protection diodes load the shared UART lines. Both
   ports on, together, always.
5. **Reversed/unknown LiPo polarity** — Adafruit JST polarity only;
   reversed packs destroy the charger.

## Smoke-test procedure (before full integration)

Run in order; stop at any failure.

1. **Bank + HUZZAH32 alone** (no Lite wire, no M0): boots, WiFi joins,
   OLED lights. Measure HUZZAH32 `USB` pin: 4.75–5.25 V.
2. **Bank + M0 alone**: boots, SD init OK, green-LED heartbeat.
3. **Lite alone on the USB adapter** (Feathers unplugged): u-center
   connects, config checklist done, saved to flash, power-cycle verified.
   Unplug adapter.
4. **HUZZAH32 + Lite, power only** (wires 1 and 4 from `wiring.md`; no
   data wires yet): Lite power LED on; F9P LED behavior normal; HUZZAH32
   still boots and runs WiFi (confirms port-1 budget). Check 5 V at JST
   pin 1 under WiFi load.
5. **Add data wires 2 and 3 (HUZZAH32 leg only)**: NMEA visible in the
   ESP32 console at 115200.
6. **Add the M0 tap (wire 3's second leg) + status link (5, 6, 7)**, both
   Feathers powered: M0 sees the same NMEA/UBX stream; HUZZAH32 reception
   unchanged (byte counters match test 5 over 10 min).
7. Full stack soak, 1 h: no brownouts (ESP32 brownout resets show in the
   boot log), SD file grows, bank stays on.
