#include "status_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

// Portable NUL-terminating copy (no strlcpy dependency -- not guaranteed present on every
// Arduino core this file compiles under). Same pattern as Metro's settings.cpp copyStr().
void copyStr(char* dst, size_t dstLen, const char* src) {
  strncpy(dst, src, dstLen - 1);
  dst[dstLen - 1] = '\0';
}

size_t finish(char* out, size_t outLen, const char* body) {
  int n = snprintf(out, outLen, "$%s*%02X\r\n", body, statusLinkChecksum(body));
  if (n < 0 || (size_t)n >= outLen) return 0;
  return (size_t)n;
}

// Verifies "$BODY*CS" (no \r\n -- the caller strips it) and, on success, NUL-terminates
// `line` at the '*' so parsers see just the body (still including the leading '$' and type
// word). Returns false on a bad or missing checksum.
bool verifyAndStrip(char* line) {
  if (line[0] != '$') return false;
  char* star = strrchr(line, '*');
  if (!star || strlen(star) < 3) return false;
  uint8_t rx = (uint8_t)strtoul(star + 1, nullptr, 16);
  *star = '\0';
  uint8_t calc = statusLinkChecksum(line + 1);  // body excludes the leading '$'
  return calc == rx;
}

}  // namespace

uint8_t statusLinkChecksum(const char* body) {
  uint8_t cs = 0;
  for (const char* p = body; *p; p++) cs ^= (uint8_t)*p;
  return cs;
}

size_t buildTimeSync(char* out, size_t outLen, uint16_t year, uint8_t month, uint8_t day,
                      uint8_t hour, uint8_t minute, uint8_t second) {
  char body[48];
  snprintf(body, sizeof(body), "TSYNC,%04u,%02u,%02u,%02u,%02u,%02u", year, month, day, hour,
           minute, second);
  return finish(out, outLen, body);
}

size_t buildLogCtl(char* out, size_t outLen, bool start) {
  char body[24];
  snprintf(body, sizeof(body), "LOGCTL,%s", start ? "START" : "STOP");
  return finish(out, outLen, body);
}

size_t buildShutdown(char* out, size_t outLen) {
  return finish(out, outLen, "SHUTDOWN");
}

size_t buildM0Status(char* out, size_t outLen, bool logging, const char* fileName,
                      uint32_t bytesWritten, uint32_t sdFreeKB) {
  char body[80];
  snprintf(body, sizeof(body), "MSTA,%d,%s,%lu,%lu", logging ? 1 : 0,
           (fileName && fileName[0]) ? fileName : "-", (unsigned long)bytesWritten,
           (unsigned long)sdFreeKB);
  return finish(out, outLen, body);
}

bool parseTimeSync(const char* lineIn, TimeSyncMsg& out) {
  char line[LineAssembler::kMaxLine];
  copyStr(line, sizeof(line), lineIn);
  if (!verifyAndStrip(line)) return false;
  unsigned y, mo, d, h, mi, s;
  if (sscanf(line, "$TSYNC,%u,%u,%u,%u,%u,%u", &y, &mo, &d, &h, &mi, &s) != 6) return false;
  out.year = (uint16_t)y;
  out.month = (uint8_t)mo;
  out.day = (uint8_t)d;
  out.hour = (uint8_t)h;
  out.minute = (uint8_t)mi;
  out.second = (uint8_t)s;
  return true;
}

bool parseLogCtl(const char* lineIn, bool& start) {
  char line[LineAssembler::kMaxLine];
  copyStr(line, sizeof(line), lineIn);
  if (!verifyAndStrip(line)) return false;
  if (!strcmp(line, "$LOGCTL,START")) {
    start = true;
    return true;
  }
  if (!strcmp(line, "$LOGCTL,STOP")) {
    start = false;
    return true;
  }
  return false;
}

bool parseShutdown(const char* lineIn) {
  char line[LineAssembler::kMaxLine];
  copyStr(line, sizeof(line), lineIn);
  if (!verifyAndStrip(line)) return false;
  return !strcmp(line, "$SHUTDOWN");
}

bool parseM0Status(const char* lineIn, M0StatusMsg& out) {
  char line[LineAssembler::kMaxLine];
  copyStr(line, sizeof(line), lineIn);
  if (!verifyAndStrip(line)) return false;
  int logging = 0;
  char fname[32];
  unsigned long bw = 0, freeKB = 0;
  if (sscanf(line, "$MSTA,%d,%31[^,],%lu,%lu", &logging, fname, &bw, &freeKB) != 4)
    return false;
  out.logging = logging != 0;
  copyStr(out.fileName, sizeof(out.fileName), fname);
  out.bytesWritten = bw;
  out.sdFreeKB = freeKB;
  return true;
}

bool LineAssembler::feed(uint8_t b, char* lineOut, size_t lineOutLen) {
  if (b == '\n') {
    bool ok = false;
    if (!_overflowed && _len > 0) {
      if (_buf[_len - 1] == '\r') _len--;  // strip a trailing CR if present
      _buf[_len] = '\0';
      copyStr(lineOut, lineOutLen, _buf);
      ok = true;
    }
    _len = 0;
    _overflowed = false;
    return ok;
  }
  if (_len < kMaxLine - 1) {
    _buf[_len++] = (char)b;
  } else {
    _overflowed = true;  // line too long -- drop it, resync at the next '\n'
  }
  return false;
}
