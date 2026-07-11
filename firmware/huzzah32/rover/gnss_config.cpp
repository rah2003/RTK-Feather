// ZED-F9P configuration via UBX-CFG-VALSET, u-blox Interface Description (HPG 1.32+) key
// names cited on every line -- cross-checked against this project's own bench research in
// docs/hardware/ucenter-config.md (key IDs verified against the pyubx2 configdb mirror and
// PaulZC's UBX.md). Adapted from the Metro project's gnss_config.cpp
// (rah2003/SWMaps-propertylines), simplified to UART1-only: Topology B forecloses the I2C
// path entirely (the Lite has no I2C pads), so there is no transport branch here.
//
// Important Topology-B-specific point: this MCU enables RXM_RAWX/RXM_SFRBX output on UART1
// even though IT never logs them -- the M0 Adalogger taps the same physical wire
// (docs/hardware/wiring.md wire 3) and passively extracts those frames for its own SD log.
// If this config didn't turn them on, the M0 would have nothing to log.
//
// Baud: 115200, not Metro's 460800 -- docs/hardware/topology.md's link budget was computed
// specifically for the shared UART1 tap (HUZZAH32 + M0 both listening) and the M0's 32 KiB
// RAM ring-buffer math; 115200 leaves ~2x margin there. Do not raise this without redoing
// that math.
#include "gnss_config.h"

#include "features.h"
#include "settings.h"

namespace {

constexpr uint8_t kLayers = VAL_LAYER_ALL;

bool applyPorts(SFE_UBLOX_GNSS_SERIAL& g) {
  bool ok = g.newCfgValset(kLayers);
  // ucenter-config.md §2 -- protocols on UART1. Input: UBX (config/poll) + RTCM3X
  // (corrections) only; NMEA input is never sent by this ESP32, so it's disabled.
  ok &= g.addCfgValset(UBLOX_CFG_UART1INPROT_UBX, 1);      // CFG-UART1INPROT-UBX
  ok &= g.addCfgValset(UBLOX_CFG_UART1INPROT_NMEA, 0);     // CFG-UART1INPROT-NMEA
  ok &= g.addCfgValset(UBLOX_CFG_UART1INPROT_RTCM3X, 1);   // CFG-UART1INPROT-RTCM3X
  // Output: UBX (RAWX/SFRBX/NAV-PVT) + NMEA (GGA/RMC/GST/GSA/GSV). RTCM3X out stays off --
  // that's Base mode's job, and only on UART2 (config-only, brief §2), never UART1.
  ok &= g.addCfgValset(UBLOX_CFG_UART1OUTPROT_UBX, 1);     // CFG-UART1OUTPROT-UBX
  ok &= g.addCfgValset(UBLOX_CFG_UART1OUTPROT_NMEA, 1);    // CFG-UART1OUTPROT-NMEA
  ok &= g.addCfgValset(UBLOX_CFG_UART1OUTPROT_RTCM3X, 0);  // CFG-UART1OUTPROT-RTCM3X
  // UART2/XBee: deliberately absent here (reserved for the future radio, brief §2).
  ok &= g.sendCfgValset();
  return ok;
}

bool applyMessages(SFE_UBLOX_GNSS_SERIAL& g) {
  bool ok = g.newCfgValset(kLayers);
  // Raw-data set for the M0 tap (ucenter-config.md §3; PaulZC RAWX_Logger pattern):
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_RAWX_UART1, 1);   // CFG-MSGOUT-UBX_RXM_RAWX_UART1
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_UBX_RXM_SFRBX_UART1, 1);  // CFG-MSGOUT-UBX_RXM_SFRBX_UART1
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_UBX_NAV_PVT_UART1, 1);    // CFG-MSGOUT-UBX_NAV_PVT_UART1
  // NMEA set for this MCU's own status/GGA-upstream use (ucenter-config.md §4):
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GGA_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_RMC_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GST_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSA_UART1, 1);
  ok &= g.addCfgValset(UBLOX_CFG_MSGOUT_NMEA_ID_GSV_UART1, 5);  // every 5th epoch (link margin)
  ok &= g.addCfgValset(UBLOX_CFG_NMEA_HIGHPREC, 1);              // CFG-NMEA-HIGHPREC
  ok &= g.sendCfgValset();
  return ok;
}

bool applyNav(SFE_UBLOX_GNSS_SERIAL& g) {
  bool ok = g.newCfgValset(kLayers);
  ok &= g.addCfgValset(UBLOX_CFG_RATE_MEAS, 1000);  // CFG-RATE-MEAS: 1000 ms = 1 Hz
  ok &= g.addCfgValset(UBLOX_CFG_RATE_NAV, 1);       // CFG-RATE-NAV: 1 measurement/solution
  ok &= g.addCfgValset(UBLOX_CFG_NAVSPG_INFIL_MINELEV, g_settings.elevMaskDeg);
  // CFG-NAVSPG-INFIL_MINELEV -- inherited Metro default (12 deg); ucenter-config.md's own
  // bench checklist proposed 10 deg as a starting point, superseded by this project decision.
  // Constellations -- inherited Metro default: GPS+GLO+GAL+BDS; SBAS off (conflicts with
  // RTK, marginal at 61N); QZSS off (not visible from Alaska).
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_GPS_ENA, 1);   // CFG-SIGNAL-GPS_ENA
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_GLO_ENA, 1);   // CFG-SIGNAL-GLO_ENA
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_GAL_ENA, 1);   // CFG-SIGNAL-GAL_ENA
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_BDS_ENA, 1);   // CFG-SIGNAL-BDS_ENA
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_SBAS_ENA, 0);  // CFG-SIGNAL-SBAS_ENA
  ok &= g.addCfgValset(UBLOX_CFG_SIGNAL_QZSS_ENA, 0);  // CFG-SIGNAL-QZSS_ENA
  ok &= g.sendCfgValset();
  return ok;
}

bool applyRoverTmode(SFE_UBLOX_GNSS_SERIAL& g) {
  // Make sure a previous base-mode session (Phase 3, not built yet) can't leave TMODE latched
  // on -- always force it off in this Rover-only v1.
  bool ok = g.newCfgValset(kLayers);
  ok &= g.addCfgValset(UBLOX_CFG_TMODE_MODE, 0);  // CFG-TMODE-MODE: 0 = disabled (rover)
  ok &= g.sendCfgValset();
  return ok;
}

}  // namespace

bool gnssApplyProjectConfig(SFE_UBLOX_GNSS_SERIAL& gnss) {
  bool ok = applyPorts(gnss);
  ok &= applyMessages(gnss);
  ok &= applyNav(gnss);
  ok &= applyRoverTmode(gnss);  // FEATURE_BASE is 0 for v1 -- no Base branch to gate
  return ok;
}

bool gnssFactoryRecover(SFE_UBLOX_GNSS_SERIAL& gnss) {
  Serial.println(F("[gnss] factory reset (UBX-CFG-CFG clear/load defaults)..."));
  gnss.factoryDefault();
  delay(5000);  // module reboots; give it time before we talk to it again

  // Factory defaults put UART1 back to 38400 -- redo the baud dance from gnss_task's connect.
  Serial1.updateBaudRate(38400);
  if (!gnss.isConnected()) {
    Serial.println(F("[gnss] F9P not answering at 38400 after reset"));
    return false;
  }
  gnss.newCfgValset(VAL_LAYER_ALL);
  gnss.addCfgValset(UBLOX_CFG_UART1_BAUDRATE, 115200);  // CFG-UART1-BAUDRATE
  gnss.sendCfgValset(250);
  delay(200);
  Serial1.updateBaudRate(115200);

  if (!gnss.isConnected()) {
    Serial.println(F("[gnss] F9P not answering after reset"));
    return false;
  }
  Serial.println(F("[gnss] reapplying project config..."));
  return gnssApplyProjectConfig(gnss);
}
