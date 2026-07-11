#include "ubx_extractor.h"

void UbxExtractor::resetParse() {
  _state = State::SYNC1;
  _frameLen = 0;
  _payloadLen = 0;
  _ckA = _ckB = 0;
}

void UbxExtractor::feed(uint8_t b) {
  switch (_state) {
    case State::SYNC1:
      _state = (b == 0xB5) ? State::SYNC2 : State::SYNC1;
      break;

    case State::SYNC2:
      _state = (b == 0x62) ? State::CLASS : State::SYNC1;
      break;

    case State::CLASS:
      _frame[0] = b;
      _frameLen = 1;
      _ckA = b;
      _ckB = _ckA;
      _state = State::ID;
      break;

    case State::ID:
      _frame[1] = b;
      _frameLen = 2;
      _ckA += b;
      _ckB += _ckA;
      _state = State::LEN1;
      break;

    case State::LEN1:
      _frame[2] = b;
      _frameLen = 3;
      _payloadLen = b;
      _ckA += b;
      _ckB += _ckA;
      _state = State::LEN2;
      break;

    case State::LEN2:
      _frame[3] = b;
      _frameLen = 4;
      _payloadLen |= ((uint16_t)b << 8);
      _ckA += b;
      _ckB += _ckA;
      if (_payloadLen > kMaxFrame - 8) {
        resetParse();  // implausible length for this project's messages -- bail, resync
        break;
      }
      _state = (_payloadLen == 0) ? State::CK_A : State::PAYLOAD;
      break;

    case State::PAYLOAD:
      _frame[_frameLen++] = b;
      _ckA += b;
      _ckB += _ckA;
      if (_frameLen == 4u + _payloadLen) _state = State::CK_A;
      break;

    case State::CK_A:
      _rxCkA = b;
      _state = State::CK_B;
      break;

    case State::CK_B:
      if (_rxCkA == _ckA && b == _ckB) {
        pushFrameToRing();
        _framesExtracted++;
      } else {
        _checksumErrors++;
      }
      resetParse();
      break;
  }
}

void UbxExtractor::pushFrameToRing() {
  // Whole frame, sync-to-checksum, so what lands on SD is byte-identical to the source
  // (test-plan.md Stage 2 item 2): B5 62 + class/id/len/payload + ckA/ckB.
  uint8_t header[2] = {0xB5, 0x62};
  ringPush(header, 2);
  ringPush(_frame, _frameLen);
  uint8_t cksum[2] = {_ckA, _ckB};
  ringPush(cksum, 2);
}

void UbxExtractor::ringPush(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (_ringUsed >= kRingSize) break;  // full: drop rather than corrupt; caller must drain faster
    _ring[_ringHead] = data[i];
    _ringHead = (_ringHead + 1) % kRingSize;
    _ringUsed++;
  }
  if (_ringUsed > _ringHighWater) _ringHighWater = _ringUsed;
}

size_t UbxExtractor::drain(uint8_t* out, size_t maxLen) {
  size_t n = maxLen < _ringUsed ? maxLen : _ringUsed;
  for (size_t i = 0; i < n; i++) {
    out[i] = _ring[_ringTail];
    _ringTail = (_ringTail + 1) % kRingSize;
  }
  _ringUsed -= n;
  return n;
}
