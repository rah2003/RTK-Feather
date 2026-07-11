#include "shared.h"

RingBuf<8192> rtcmRing;

LinkStatus g_link;

static GnssStatus s_gnss;
static M0Status s_m0;
static SemaphoreHandle_t s_gnssMutex;
static SemaphoreHandle_t s_m0Mutex;

void statusInit() {
  s_gnssMutex = xSemaphoreCreateMutex();
  s_m0Mutex = xSemaphoreCreateMutex();
}

void statusSetGnss(const GnssStatus& s) {
  xSemaphoreTake(s_gnssMutex, portMAX_DELAY);
  s_gnss = s;
  xSemaphoreGive(s_gnssMutex);
}

GnssStatus statusGetGnss() {
  GnssStatus out;
  xSemaphoreTake(s_gnssMutex, portMAX_DELAY);
  out = s_gnss;
  xSemaphoreGive(s_gnssMutex);
  return out;
}

void statusSetM0(const M0Status& s) {
  xSemaphoreTake(s_m0Mutex, portMAX_DELAY);
  s_m0 = s;
  xSemaphoreGive(s_m0Mutex);
}

M0Status statusGetM0() {
  M0Status out;
  xSemaphoreTake(s_m0Mutex, portMAX_DELAY);
  out = s_m0;
  xSemaphoreGive(s_m0Mutex);
  return out;
}

uint32_t correctionAgeMs() {
  uint32_t last = g_link.lastRtcmMs;
  if (last == 0) return UINT32_MAX;
  return millis() - last;
}
