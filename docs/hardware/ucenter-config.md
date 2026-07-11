# F9P Pre-Configuration Checklist (u-center over the Lite's USB adapter)

**Bench rule (read first):** the Lite's XBee-to-USB adapter is an FTDI
bridge to **UART1** — the same port the Feathers use. Do this procedure
with the **JST-GH pigtail unplugged** (no HUZZAH32 TX on the line). Never
have both connected. See `wiring.md`.

Setup: Lite seated on the XBee-to-USB adapter → PC running u-center
(classic u-center for F9P, not u-center 2 unless it supports your FW).
Antenna connected with sky view if you want to watch fixes; not required
for config. Initial connection baud: **38400** (F9P UART1 default).

Use **View → Generation 9 Configuration View** (VALSET/VALGET). Apply each
group to layer **RAM** first, verify behavior, then re-apply to **Flash**
(or select RAM+BBR+Flash at once). Key names below are from the u-blox F9
HPG interface description; IDs verified against the u-blox configuration
database mirror ([pyubx2 configdb](https://github.com/semuconsulting/pyubx2/blob/master/src/pyubx2/ubxtypes_configdb.py),
[PaulZC UBX.md](https://github.com/PaulZC/F9P_RAWX_Logger/blob/master/UBX.md)) —
re-confirm against the HPG 1.32+ PDF when convenient.

## 1. Identity / firmware

- [ ] UBX-MON-VER: record FW version (want HPG 1.32 or later; PaulZC's 2024
      logger recommends 1.32). Record for the project log.

## 2. UART1 port

| Key | ID | Value |
|---|---|---|
| `CFG-UART1-BAUDRATE` | `0x40520001` | **115200** (project default; 230400 is the headroom option — see `topology.md`) |
| `CFG-UART1INPROT-UBX` | `0x10730001` | 1 (config/poll from ESP32) |
| `CFG-UART1INPROT-NMEA` | `0x10730002` | 0 |
| `CFG-UART1INPROT-RTCM3X` | `0x10730004` | **1** (corrections in) |
| `CFG-UART1OUTPROT-UBX` | `0x10740001` | 1 |
| `CFG-UART1OUTPROT-NMEA` | `0x10740002` | 1 |
| `CFG-UART1OUTPROT-RTCM3X` | `0x10740004` | 0 (Rover; Base uses UART2) |

After changing the baud, reconnect u-center at 115200 to continue.

## 3. Raw-data messages on UART1 (for the M0 tap)

| Key | ID | Value |
|---|---|---|
| `CFG-MSGOUT-UBX_RXM_RAWX_UART1` | `0x209102a5` | 1 |
| `CFG-MSGOUT-UBX_RXM_SFRBX_UART1` | `0x20910232` | 1 |
| `CFG-MSGOUT-UBX_TIM_TM2_UART1` | (look up in Config View) | 1 — optional |

## 4. NMEA set on UART1 (for the ESP32: SW Maps + GGA-to-caster)

NMEA is on by default at 1 Hz on UART1. Confirm/adjust per-message rates
(`CFG-MSGOUT-NMEA_ID_*_UART1` keys in the Config View):

- [ ] GGA = 1, RMC = 1, GSA = 1, GST = 1 (accuracy figures for SW Maps)
- [ ] GSV = 1 (or 5 = every 5th epoch if link margin ever tightens)
- [ ] VTG = 0 unless wanted
- [ ] High-precision mode: `CFG-NMEA-HIGHPREC` = 1 (7-digit lat/lon
      minutes; PaulZC precedent)

## 5. Rates and dynamics

- [ ] `CFG-RATE-MEAS` = 1000 ms, `CFG-RATE-NAV` = 1 → 1 Hz (project
      default; keeps the M0 link budget at ~49 % worst case).
- [ ] Dynamic model (`CFG-NAVSPG-DYNMODEL`): 0 (portable) or 2
      (stationary) for base work; PaulZC used airborne-1g for kinematic
      rover logging — decide per use, default portable.
- [ ] Elevation mask (`CFG-NAVSPG-INFIL_MINELEV`): propose **10–15°**
      (Anchorage-area default from the project brief) — set 10 to start.

## 6. Ports we deliberately leave alone

- [ ] **UART2: do not disable, do not reconfigure** beyond Base-mode RTCM
      output later (config-only; socket stays physically empty).
- [ ] I2C/USB interfaces: leave at defaults (the Lite doesn't expose them;
      disabling buys nothing and complicates recovery).

## 7. Save and verify

- [ ] Re-apply the full set to the **Flash** layer (Generation 9 view:
      tick RAM + BBR + Flash, Send).
- [ ] Power-cycle the Lite. Reconnect at 115200. VALGET a spot-check
      (RAWX_UART1, BAUDRATE) from the **Flash** layer and confirm values.
- [ ] Watch the packet console: RAWX + SFRBX + the NMEA set streaming at
      1 Hz, no other UBX chatter.
- [ ] Unplug the USB adapter **before** connecting the JST pigtail.

## Recovery

If the board is ever mis-configured into silence: connect via the USB
adapter, u-center → UBX-CFG-CFG "Revert to default configuration" (or
Generation 9 `CFG-CFG` clear), then redo this checklist.
