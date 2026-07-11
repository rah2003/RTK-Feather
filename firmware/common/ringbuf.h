// Lock-free single-producer / single-consumer byte ring buffer.
//
// Ported near-verbatim from the Metro project (rah2003/SWMaps-propertylines,
// firmware/src/rtkbridge/ringbuf.h) per the project brief's "reuse wherever it transfers"
// directive -- this primitive is exactly what HUZZAH32's cross-core NTRIP/GNSS split needs
// (dual-core ESP32, same as the Metro board). Not currently used on the M0 side: that MCU is
// single-core superloop (project brief §4), so its own buffering
// (common/ubx_extractor.h's internal ring) is deliberately non-atomic instead.
//
// Safe across two FreeRTOS tasks on different cores as long as exactly one task writes and
// exactly one task reads: head is only advanced by the producer, tail only by the consumer.
// Indices are std::atomic with release/acquire ordering -- the release store of _head
// publishes the payload bytes written before it, and the acquire load on the consumer side
// makes them visible (plain `volatile` would NOT order the non-volatile _buf stores against
// the index publish, allowing a cross-core reader to see the new index before the data).
// No String, no malloc, no locks in the hot path.
#pragma once
#include <Arduino.h>

#include <atomic>

template <size_t N>
class RingBuf {
  static_assert((N & (N - 1)) == 0, "N must be a power of two");

 public:
  size_t write(const uint8_t* data, size_t len) {
    size_t h = _head.load(std::memory_order_relaxed);  // producer owns head
    size_t t = _tail.load(std::memory_order_acquire);   // consumer's progress
    size_t space = N - (h - t);
    if (len > space) len = space;  // drop the tail of an oversized write; caller checks count
    for (size_t i = 0; i < len; i++) {
      _buf[h & (N - 1)] = data[i];
      h++;
    }
    _head.store(h, std::memory_order_release);  // publish after the bytes are in place
    return len;
  }

  size_t read(uint8_t* out, size_t maxLen) {
    size_t t = _tail.load(std::memory_order_relaxed);  // consumer owns tail
    size_t h = _head.load(std::memory_order_acquire);   // producer's published bytes
    size_t avail = h - t;
    if (maxLen > avail) maxLen = avail;
    for (size_t i = 0; i < maxLen; i++) {
      out[i] = _buf[t & (N - 1)];
      t++;
    }
    _tail.store(t, std::memory_order_release);  // free the space after the bytes are copied
    return maxLen;
  }

  size_t available() const {
    return _head.load(std::memory_order_acquire) - _tail.load(std::memory_order_acquire);
  }
  size_t free() const { return N - available(); }
  size_t capacity() const { return N; }
  void clear() {  // consumer-side only
    _tail.store(_head.load(std::memory_order_acquire), std::memory_order_release);
  }

 private:
  uint8_t _buf[N];
  std::atomic<size_t> _head{0};  // producer-owned
  std::atomic<size_t> _tail{0};  // consumer-owned
};
