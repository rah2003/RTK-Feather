#pragma once
#include <Arduino.h>

// Passive UBX frame extractor for a mixed NMEA+UBX byte stream -- this is what the M0 sees
// on the shared-listener UART tap in Topology B (docs/hardware/topology.md). Feed bytes one
// at a time via feed(); complete, checksum-valid frames (sync B5 62 .. class .. id ..
// len(2) .. payload .. ckA ckB, u-blox 8-bit Fletcher checksum) are pushed whole into an
// internal ring buffer for the caller to drain to SD. NMEA ASCII lines and anything that
// fails checksum are silently discarded -- this is a logger, not a validator for the NMEA
// side (the HUZZAH32 owns that).
//
// Single-threaded by design (superloop, per project brief §4): feed() and drain() are both
// called from loop(), never from an ISR, so plain counters are safe -- no atomics needed
// (unlike the ESP32 side's cross-core ring buffers).
class UbxExtractor {
 public:
  static constexpr size_t kMaxFrame = 3072;  // > worst-case RAWX frame, ~2.3 KiB (topology.md)
  // 12 KiB, down from the original 16 KiB: the 4 KiB was re-spent on the SAMD21 core's UART
  // buffers (-DSERIAL_BUFFER_SIZE=1024 in platformio.ini) after review found the 350 B core
  // default was the true loss point during SD write stalls -- this ring only ever sees bytes
  // that already survived the UART buffer. 12 KiB still absorbs ~1.07 s at 11,520 B/s,
  // ~4.3x the budgeted 250 ms worst-case SD stall (topology.md math).
  static constexpr size_t kRingSize = 12288;

  void feed(uint8_t b);

  // Drains up to `maxLen` bytes of complete frame data into `out`. Returns bytes drained.
  size_t drain(uint8_t* out, size_t maxLen);

  size_t ringUsed() const { return _ringUsed; }
  size_t ringHighWater() const { return _ringHighWater; }
  uint32_t framesExtracted() const { return _framesExtracted; }
  uint32_t checksumErrors() const { return _checksumErrors; }
  uint32_t framesDropped() const { return _framesDropped; }  // whole frames dropped, ring full

 private:
  enum class State { SYNC1, SYNC2, CLASS, ID, LEN1, LEN2, PAYLOAD, CK_A, CK_B };
  State _state = State::SYNC1;

  uint8_t _frame[kMaxFrame];  // class, id, len(2), payload -- header/checksum added on push
  size_t _frameLen = 0;
  uint16_t _payloadLen = 0;
  uint8_t _ckA = 0, _ckB = 0;      // running checksum over class..payload
  uint8_t _rxCkA = 0;              // checksum byte A read from the stream

  uint8_t _ring[kRingSize];
  size_t _ringHead = 0, _ringTail = 0, _ringUsed = 0, _ringHighWater = 0;

  uint32_t _framesExtracted = 0;
  uint32_t _checksumErrors = 0;
  uint32_t _framesDropped = 0;

  void resetParse();
  void pushFrameToRing();
  void ringPush(const uint8_t* data, size_t len);
};
