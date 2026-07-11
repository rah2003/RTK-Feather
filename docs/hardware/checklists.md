# Per-Board Bring-Up Checklists

Work through these before any inter-board wiring. Sources are cited so
each item can be re-verified against current vendor docs.

## simpleRTK2B Lite (ZED-F9P)

- [ ] Identify SKU: AS-RTK2B-LIT-L1L2-SMA-00 (mini-USB adapter) or -01
      (USB-C adapter) — both are SMA antenna
      ([product page](https://www.ardusimple.com/product/simplertk2blite/)).
- [ ] **Photo-check for I2C pads** (expected: none — only TIMEPULSE,
      RTK_STAT, GEOFENCE are documented). Confirms Topology B.
- [ ] Locate JST-GH **pin 1 marking** (white square / printed "1") and
      record which end of your pigtail it is. Do not trust wire colors.
- [ ] JST pinout confirmed on the silk/docs: 1=5V_IN, 2=UART1 RX (F9P
      input), 3=UART1 TX (F9P output), 4/5=NC, 6=GND
      ([hookup guide](https://www.ardusimple.com/simplertk2blite-hookup-guide/)).
- [ ] **Top XBee socket empty** and kept empty (UART2, future radio;
      socket VCC is a 3.3 V/250 mA output — never feed power in).
- [ ] Antenna: dual-band L1/L2 **active** antenna on SMA (board supplies
      3.3 V ≤ 75 mA); antenna has sky view or ground-plane plate for bench.
- [ ] Power inputs understood: Pixhawk 5V (4.5–5.5 V), USB adapter, or
      XBee-header 3.3 V (3.0–3.6 V); simultaneous sources are sanctioned by
      ArduSimple — but **UART1 data contention is not** (see `wiring.md`).
- [ ] u-center pre-configuration done and saved to flash
      (`ucenter-config.md`), verified to survive a power cycle.

## HUZZAH32 Feather (ESP32-WROOM)

- [ ] Board boots, USB serial console at 115200 (`Serial`).
- [ ] Red LED (GPIO13) blink test.
- [ ] I2C scan on SDA=23/SCL=22 finds OLED at **0x3C** (Wing stacked).
      HUZZAH32 has no onboard I2C pullups
      ([pinouts](https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts));
      the Wing supplies them.
- [ ] Pin plan pinned: Serial1 = GPIO17 TX / GPIO16 RX (to F9P);
      Serial2 remapped RX=27 / TX=33 (status link). Confirm none of
      34/36/39 (input-only) or 12 (boot pulldown — "use as output only",
      [pinouts](https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts))
      are in the plan.
- [ ] Strapping cautions noted: GPIO15 = OLED button A (MTDO) — don't
      hold button A through a reset; GPIO0/2 aren't on the header.
- [ ] WiFi join + NTRIP smoke test (test plan Stage 1) from USB power.
- [ ] Power budget acknowledged: AP2112 500 mA peak regulator, ~250 mA
      budgeted to the WROOM32 — **no external loads on 3V3 beyond the OLED**
      ([power management](https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/power-management)).
- [ ] ADC2 pins unusable for analog once WiFi starts (only relevant if
      analog sensing is ever added).

## Feather M0 Adalogger (SAMD21)

- [ ] Board boots; green LED (D8) and red LED (D13) tests.
- [ ] microSD formatted (FAT32), inserted; SdFat init at CS=**D4**
      succeeds; card-detect **D7** reads high with card, low without
      (input-pullup)
      ([pinouts](https://learn.adafruit.com/adafruit-feather-m0-adalogger/pinouts)).
- [ ] Bench write test ≥ 4 MB via SdFat, 512-byte chunks.
- [ ] **SERCOM plan pinned (the classic SAMD21 footgun — spelled out):**
      - Serial1 (GNSS tap RX): SERCOM0, D0=PA11=pad 3 (RX), D1=PA10=pad 2
        (TX, **firmware-idle and physically unconnected**)
        ([variant.cpp](https://github.com/adafruit/ArduinoCore-samd/blob/master/variants/feather_m0/variant.cpp)).
      - Serial2 (status link): SERCOM1 — D11=PA16=SERCOM1 **pad 0** = RX,
        D10=PA18=SERCOM1 **pad 2** = TX, peripheral mux **C**
        (`PIO_SERCOM`); constructor
        `Uart Serial2(&sercom1, 11, 10, SERCOM_RX_PAD_0, UART_TX_PAD_2)`
        + `SERCOM1_Handler(){Serial2.IrqHandler();}` + `pinPeripheral()`
        after `begin()`
        ([Adafruit SERCOM guide](https://learn.adafruit.com/using-atsamd21-sercom-to-add-more-spi-i2c-serial-ports/creating-a-new-serial)).
      - Reserved: SERCOM3 = Wire (D20/21), SERCOM4 = SPI/SD, SERCOM5 =
        core-defined hidden Serial5 (handler already exists — never define
        `SERCOM5_Handler`). TX is only legal on SERCOM pads 0 or 2
        ([SERCOM.h](https://github.com/adafruit/ArduinoCore-samd/blob/master/cores/arduino/SERCOM.h)).
- [ ] UBX replay test from PC (test plan Stage 2) — byte-identical frames.
- [ ] Battery sense A7 (÷2 divider) read sanity if a LiPo is fitted; PaulZC
      precedent closes files below 3.55 V.

## OLED FeatherWing — variant identification (QUESTIONS.md Q2)

| | 128×32 (#2900) | 128×64 (#4650) |
|---|---|---|
| Driver | SSD1306 | SH1107 |
| Address | 0x3C fixed | 0x3C default (jumper → 0x3D) |
| Library | Adafruit_SSD1306 | Adafruit_SH110X (`Adafruit_SH1107`) |
| Init quirk | reset pin `-1` (auto-reset) | construct **64×128 portrait** + `setRotation(1)`; init uses display-offset 0x60 — generic SSD1306 drivers show garbage |

Both variants ([2900 guide](https://learn.adafruit.com/adafruit-oled-featherwing/pinouts),
[4650 guide](https://learn.adafruit.com/adafruit-128x64-oled-featherwing/pinouts)):
buttons on ESP32 Feather are **A=GPIO15, B=GPIO32, C=GPIO14**; button B has
a 100 kΩ onboard pullup, A and C need `INPUT_PULLUP`; the Wing's reset
button resets the **whole Feather**, not just the display.

- [ ] Identify variant (product number on bag/silk, or count pixel rows).
- [ ] Run the matching Adafruit example; confirm buttons A/B/C register.
