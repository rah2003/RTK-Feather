// The GNSS heart of this MCU -- adapted from the Metro project's gnss_task.cpp
// (rah2003/SWMaps-propertylines), UART1-only (Topology B has no I2C path) and with the
// RAWX/SFRBX file-buffer/SD-logging machinery removed entirely: that's the M0's job over
// the passive tap, not this MCU's. This task still turns those messages ON on UART1
// (gnss_config.cpp) -- it just never asks the SparkFun library to buffer or store them
// itself, which also means skipping Metro's 16 KiB file-buffer allocation (setFileBufferSize)
// -- irrelevant RAM cost removed along with the irrelevant feature.
//
//   in : rtcmRing -> pushRawData()             (RTCM injection, brief §2)
//   out: processNMEA weak override -> latest GGA (NTRIP upstream) + status
//   out: NAV-PVT callback -> GnssStatus        (OLED status, correction-age display, filenames)
#include "gnss_task.h"

#include <SparkFun_u-blox_GNSS_v3.h>
#include <esp_task_wdt.h>

#include "features.h"
#include "gnss_config.h"
#include "pins.h"
#include "settings.h"
#include "shared.h"

static SFE_UBLOX_GNSS_SERIAL gnss;

// F9P UART1 factory default is 38400; project default is 115200 (docs/hardware/topology.md
// link budget, NOT Metro's 460800 -- that number was sized for Metro's single-listener UART,
// this one has to share headroom with the M0's tap and 32 KiB RAM). Persisted to flash so
// subsequent boots connect on the first try.
static constexpr uint32_t kUart1Baud = 115200;
static constexpr uint32_t kUart1DefaultBaud = 38400;

// ---------------------------------------------------------------------------
// NMEA path -- identical pattern to Metro's: processNMEA is a WEAK member function in the
// SparkFun v3 library (not virtual); defining it replaces the library's no-op. Complete
// sentences are matched here only for the GGA/status extraction this MCU needs; unlike
// Metro there's no nmeaRing (no BLE consumer in v1) to publish into.
namespace {
char nmeaLine[128];
size_t nmeaLen = 0;
char pendingGga[128] = {0};
volatile bool ggaFresh = false;

void feedNmeaByte(char c) {
  if (nmeaLen < sizeof(nmeaLine) - 1) nmeaLine[nmeaLen++] = c;
  if (c != '\n') return;
  nmeaLine[nmeaLen] = '\0';
  // "$GPGGA"/"$GNGGA": the talker varies with constellation config -- match the type only.
  if (nmeaLen > 6 && strncmp(nmeaLine + 3, "GGA", 3) == 0) {
    strncpy(pendingGga, nmeaLine, sizeof(pendingGga) - 1);
    ggaFresh = true;
  }
  nmeaLen = 0;
}
}  // namespace

void DevUBLOXGNSS::processNMEA(char c) { feedNmeaByte(c); }

namespace {

GnssStatus st;
volatile bool factoryRecoverRequested = false;

void onPvt(UBX_NAV_PVT_data_t* pvt) {
  st.fixType = pvt->fixType;
  st.carrSoln = pvt->flags.bits.carrSoln;
  st.numSV = pvt->numSV;
  st.latDeg = pvt->lat * 1e-7;
  st.lonDeg = pvt->lon * 1e-7;
  st.hMslM = pvt->hMSL * 1e-3;
  st.hAccMm = pvt->hAcc;  // NAV-PVT hAcc/vAcc are in mm
  st.vAccMm = pvt->vAcc;
  st.pdop = pvt->pDOP * 0.01f;
  st.timeValid = pvt->valid.bits.validDate && pvt->valid.bits.validTime;
  st.year = pvt->year;
  st.month = pvt->month;
  st.day = pvt->day;
  st.hour = pvt->hour;
  st.minute = pvt->min;
  st.second = pvt->sec;
}

bool connectGnss() {
  // Fast path: the project baud was already persisted to the F9P's flash on a prior boot.
  Serial1.setRxBufferSize(2048);  // must precede begin(); a full RAWX epoch can land while
                                   // this task is elsewhere servicing RTCM injection
  Serial1.begin(kUart1Baud, SERIAL_8N1, PIN_UART1_RX, PIN_UART1_TX);
  if (gnss.begin(Serial1)) return true;

  // Factory-fresh module: talk at the default 38400, raise CFG-UART1-BAUDRATE, reconnect.
  Serial1.updateBaudRate(kUart1DefaultBaud);
  if (!gnss.begin(Serial1)) return false;
  Serial.printf("[gnss] F9P at default 38400, switching to %lu\n", (unsigned long)kUart1Baud);
  gnss.newCfgValset(VAL_LAYER_ALL);
  gnss.addCfgValset(UBLOX_CFG_UART1_BAUDRATE, kUart1Baud);  // CFG-UART1-BAUDRATE
  gnss.sendCfgValset(250);  // ACK arrives at the new baud -- may be lost; don't trust the result
  delay(200);
  Serial1.updateBaudRate(kUart1Baud);
  return gnss.begin(Serial1);
}

void gnssTask(void*) {
  esp_task_wdt_add(nullptr);

  while (!connectGnss()) {
    Serial.println(F(
        "[gnss] no F9P response -- check stack seating / wiring.md wire 2/3 (hardware pack)"));
    st.f9pDetected = false;
    statusSetGnss(st);
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
  st.f9pDetected = true;
  Serial.println(F("[gnss] ZED-F9P connected (UART1 @115200, Topology B)"));

  if (!gnssApplyProjectConfig(gnss))
    Serial.println(F("[gnss] WARNING: some VALSET writes not ACKed -- check F9P firmware version"));
  gnss.setAutoPVTcallbackPtr(&onPvt);  // UBX-NAV-PVT at nav rate, no polling round-trips
  // Deliberately NOT calling setAutoRXMSFRBX/RAWX or setFileBufferSize: this MCU doesn't log
  // those messages (the M0 does, over the passive tap) -- checkUblox() still parses past
  // them safely with no handler registered, it just doesn't buffer/store them.

  uint32_t lastStatusPushMs = 0;
  uint8_t xfer[512];

  for (;;) {
    esp_task_wdt_reset();

    if (factoryRecoverRequested) {
      factoryRecoverRequested = false;
      gnssFactoryRecover(gnss);
      gnss.setAutoPVTcallbackPtr(&onPvt);
    }

    // 1) Ingest: parse whatever the F9P has queued (fires the PVT callback, feeds processNMEA).
    gnss.checkUblox();
    gnss.checkCallbacks();

    // 2) Inject RTCM from the caster. 256-byte slices keep each transaction short so ingest
    //    never starves.
    size_t n;
    while ((n = rtcmRing.read(xfer, 256)) > 0) {
      gnss.pushRawData(xfer, n);
    }

    // 3) Publish status ~5 Hz; hand fresh GGA to the NTRIP task.
    if (millis() - lastStatusPushMs >= 200) {
      lastStatusPushMs = millis();
      if (ggaFresh) {
        ggaFresh = false;
        strncpy(st.lastGga, pendingGga, sizeof(st.lastGga) - 1);
        st.lastGgaMs = millis();
      }
      statusSetGnss(st);
    }

    // 10 ms cadence: matches Metro's -- a 2 kB RAWX epoch still transits this MCU's UART1 RX
    // buffer even though we don't log it, and RTCM injection needs the same responsiveness.
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

}  // namespace

void gnssTaskStart() {
  xTaskCreatePinnedToCore(gnssTask, "gnss", 10240, nullptr, 3, nullptr, 1 /* core 1 */);
}

void gnssRequestFactoryRecover() { factoryRecoverRequested = true; }
