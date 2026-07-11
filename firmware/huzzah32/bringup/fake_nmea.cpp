#include "fake_nmea.h"

#include <math.h>

namespace {

constexpr double kFakeLatDeg = 61.2181;  // Anchorage, AK area -- test-plan.md Stage 1
constexpr double kFakeLonDeg = -149.9003;
constexpr double kFakeAltM = 30.0;

uint8_t nmeaChecksum(const char* body) {
  uint8_t cs = 0;
  for (const char* p = body; *p; p++) cs ^= (uint8_t)*p;
  return cs;
}

void emit(const char* body) {
  char line[96];
  snprintf(line, sizeof(line), "$%s*%02X\r\n", body, nmeaChecksum(body));
  Serial1.print(line);
  Serial.print(line);  // bench visibility over USB
}

void latLonFields(char* latBuf, size_t latLen, char* nsOut, char* lonBuf, size_t lonLen,
                   char* ewOut) {
  double latAbs = fabs(kFakeLatDeg);
  int latDeg = (int)latAbs;
  double latMin = (latAbs - latDeg) * 60.0;
  snprintf(latBuf, latLen, "%02d%07.4f", latDeg, latMin);
  *nsOut = kFakeLatDeg >= 0 ? 'N' : 'S';

  double lonAbs = fabs(kFakeLonDeg);
  int lonDeg = (int)lonAbs;
  double lonMin = (lonAbs - lonDeg) * 60.0;
  snprintf(lonBuf, lonLen, "%03d%07.4f", lonDeg, lonMin);
  *ewOut = kFakeLonDeg >= 0 ? 'E' : 'W';
}

}  // namespace

bool FakeNmeaSource::tick() {
  uint32_t now = millis();
  if (now - _lastTickMs < 1000) return false;
  _lastTickMs = now;

  if (++_second >= 60) {
    _second = 0;
    if (++_minute >= 60) {
      _minute = 0;
      _hour = (_hour + 1) % 24;
    }
  }
  _epoch++;

  char lat[12], lon[13], ns, ew;
  latLonFields(lat, sizeof(lat), &ns, lon, sizeof(lon), &ew);

  char body[96];
  snprintf(body, sizeof(body), "GPGGA,%02d%02d%02d.00,%s,%c,%s,%c,1,08,1.0,%.1f,M,10.0,M,,",
           _hour, _minute, _second, lat, ns, lon, ew, kFakeAltM);
  emit(body);

  snprintf(body, sizeof(body), "GPRMC,%02d%02d%02d.00,A,%s,%c,%s,%c,0.0,0.0,010126,,,A", _hour,
           _minute, _second, lat, ns, lon, ew);
  emit(body);

  if (_epoch % 5 == 0) {
    // One fixed 4-satellite GSV -- enough to exercise a multi-field sentence downstream;
    // not a realistic sky.
    emit("GPGSV,1,1,04,01,45,090,42,02,50,150,40,03,60,210,39,04,35,300,37");
  }

  return true;
}
