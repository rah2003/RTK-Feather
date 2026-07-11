#pragma once
#include <Arduino.h>

// Line-based, checksummed protocol between the HUZZAH32 and the M0 Adalogger over the
// status-link UART (docs/hardware/wiring.md wires 5/6: HUZZAH32 GPIO33 -> M0 D11, M0 D10 ->
// HUZZAH32 GPIO27, 38400 8N1 -- project brief §2: "keep the protocol trivial (line-based,
// checksummed)"). Portable (no ESP32/SAMD-specific includes) so it compiles into all four
// firmware environments; only the two rover builds actually use it.
//
// Wire format, one message per line: "$TYPE,field,field,...*CS\r\n" -- the same NMEA-style
// XOR checksum (over the bytes between '$' and '*') already used for the fake NMEA source
// (huzzah32/bringup/fake_nmea.cpp).
//
// HUZZAH32 -> M0:
//   $TSYNC,YYYY,MM,DD,hh,mm,ss      GNSS time; sent once time becomes valid, then periodically
//   $LOGCTL,START / $LOGCTL,STOP    button-driven log start/stop
//   $SHUTDOWN                       safe-shutdown request (close+flush, then safe to power off)
// M0 -> HUZZAH32:
//   $MSTA,logging,fileName,bytesWritten,sdFreeKB   periodic (~1 Hz) status

uint8_t statusLinkChecksum(const char* body);

// Builds a complete "$...*CS\r\n" line into `out` (NUL-terminated). Returns the length
// written (excluding the NUL), or 0 if it didn't fit.
size_t buildTimeSync(char* out, size_t outLen, uint16_t year, uint8_t month, uint8_t day,
                      uint8_t hour, uint8_t minute, uint8_t second);
size_t buildLogCtl(char* out, size_t outLen, bool start);
size_t buildShutdown(char* out, size_t outLen);
size_t buildM0Status(char* out, size_t outLen, bool logging, const char* fileName,
                      uint32_t bytesWritten, uint32_t sdFreeKB);

struct TimeSyncMsg {
  uint16_t year;
  uint8_t month, day, hour, minute, second;
};

struct M0StatusMsg {
  bool logging;
  char fileName[40];  // matches the M0's own filename buffer -- a longer name would make the
                      // sscanf in parseM0Status fail silently and drop the whole status line
  uint32_t bytesWritten;
  uint32_t sdFreeKB;
};

// Parsers take a line WITHOUT the trailing \r\n (LineAssembler strips it) and verify the
// checksum internally. Return true and fill `out` on success; false on a bad/missing
// checksum or a line that doesn't match the expected shape.
bool parseTimeSync(const char* line, TimeSyncMsg& out);
bool parseLogCtl(const char* line, bool& start);
bool parseShutdown(const char* line);
bool parseM0Status(const char* line, M0StatusMsg& out);

// Feed bytes one at a time from the UART. Returns true and fills `lineOut` (NUL-terminated,
// \r and \n stripped) when `b` completes a line. Lines longer than kMaxLine are dropped
// wholesale (resynced at the next '\n') rather than silently truncated-and-misparsed.
class LineAssembler {
 public:
  static constexpr size_t kMaxLine = 96;

  bool feed(uint8_t b, char* lineOut, size_t lineOutLen);

 private:
  char _buf[kMaxLine];
  size_t _len = 0;
  bool _overflowed = false;
};
