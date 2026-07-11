#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>

#include "../pins.h"
#include "../serial2_link.h"
#include "../../common/ubx_extractor.h"

// Phase 1 bring-up sketch (docs/test-plan.md Stage 2). Two independent bench checks live in
// one sketch: (1) SD + LED + status-link checks that need no PC involvement, and (2) the
// UBX replay harness, which just sits idle until bytes actually arrive on the native USB
// serial -- safe to leave running while you do the other checks.
//
// FAT32 only (checklists.md: "microSD formatted (FAT32)") -- SdFat32/File32, not the
// exFAT-capable SdFs/FsFile.

SdFat32 sd;
File32 logFile;
UbxExtractor ubx;

bool sdOk = false;
char logFileName[16];
uint32_t bytesWritten = 0;
uint32_t lastStatusMs = 0;

static void ledBringupTest() {
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_LED_GREEN, HIGH);
    delay(150);
    digitalWrite(PIN_LED_GREEN, LOW);
    digitalWrite(PIN_LED_RED, HIGH);
    delay(150);
    digitalWrite(PIN_LED_RED, LOW);
  }
}

static bool sdBringupTest() {
  pinMode(PIN_SD_CD, INPUT_PULLUP);
  bool cardPresent = digitalRead(PIN_SD_CD);  // HIGH = card present (checklists.md)
  Serial.printf("[sd] card-detect: %s\n", cardPresent ? "card present" : "NO CARD");

  if (!sd.begin(PIN_SD_CS, SD_SCK_MHZ(25))) {
    Serial.println(F("[sd] init FAILED at CS=D4"));
    return false;
  }
  Serial.println(F("[sd] init OK"));

  // Auto-numbered filename -- GNSS-time naming is a later phase (project brief §3.4); this
  // bring-up sketch has no time source yet.
  for (int i = 0; i < 1000; i++) {
    snprintf(logFileName, sizeof(logFileName), "BR%04d.UBX", i);
    if (!sd.exists(logFileName)) break;
  }

  // >=4 MB bench write test, 512-byte-aligned chunks, static buffer, no String (brief §3.4).
  File32 wtest;
  if (!wtest.open("WRTEST.BIN", O_RDWR | O_CREAT | O_TRUNC)) {
    Serial.println(F("[sd] write-test file open FAILED"));
    return false;
  }
  static uint8_t chunk[512];
  for (size_t i = 0; i < sizeof(chunk); i++) chunk[i] = (uint8_t)i;
  const uint32_t targetBytes = 4UL * 1024 * 1024;
  uint32_t written = 0;
  uint32_t t0 = millis();
  while (written < targetBytes) {
    if (wtest.write(chunk, sizeof(chunk)) != (int)sizeof(chunk)) {
      Serial.println(F("[sd] write FAILED mid-test"));
      wtest.close();
      return false;
    }
    written += sizeof(chunk);
  }
  wtest.sync();
  uint32_t elapsedMs = millis() - t0;
  wtest.close();
  sd.remove("WRTEST.BIN");
  Serial.printf("[sd] wrote %lu bytes in %lu ms (%.1f KB/s)\n", (unsigned long)written,
                (unsigned long)elapsedMs,
                elapsedMs ? (written / 1024.0) / (elapsedMs / 1000.0) : 0.0);

  if (!logFile.open(logFileName, O_RDWR | O_CREAT | O_TRUNC)) {
    Serial.println(F("[sd] .ubx log file open FAILED"));
    return false;
  }
  Serial.printf("[sd] logging replayed UBX to %s\n", logFileName);
  return true;
}

static void serial2LoopbackTest() {
  serial2Begin(38400);
  Serial.println(F("[link] Serial2 (SERCOM1, D10 TX / D11 RX) up at 38400 8N1"));
  Serial.println(F(
      "[link] bench loopback: jumper D10-D11 and watch bytes echo below; wired to the "
      "HUZZAH32 instead, this exercises the real status link"));
}

static void flushRingToSd() {
  static uint8_t chunk[512];
  size_t n;
  while ((n = ubx.drain(chunk, sizeof(chunk))) > 0) {
    if (sdOk) {
      logFile.write(chunk, n);
      bytesWritten += n;
    }
  }
}

static void closeLogSafely() {
  if (!sdOk) return;
  flushRingToSd();
  logFile.sync();
  logFile.close();
  // Two short printf calls, not one long one: the Adafruit SAMD core's Print::printf uses a
  // fixed 80-byte internal buffer (vsnprintf-bounded, so it truncates safely -- but it does
  // truncate, and this line is longer than that combined).
  Serial.printf("[sd] safe-closed %s (%lu bytes)\n", logFileName, (unsigned long)bytesWritten);
  Serial.printf("[sd] %lu frames, %lu checksum errors\n", (unsigned long)ubx.framesExtracted(),
                (unsigned long)ubx.checksumErrors());
  sdOk = false;  // stop accepting further writes until reset
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) {
  }

  Serial.println(F("=== RTK-Feather M0 Adalogger bring-up (Phase 1) ==="));
  ledBringupTest();
  sdOk = sdBringupTest();
  serial2LoopbackTest();

  Serial.println(F(
      "[ubx] ready -- replay a mixed NMEA+UBX capture into this USB-serial port now, "
      "or send \"STOP\\n\" to close the log safely"));
}

void loop() {
  // Serial2 bench loopback: anything received gets echoed straight back.
  while (Serial2.available()) {
    Serial2.write(Serial2.read());
  }

  // UBX replay harness: consume from native USB serial, extract frames, drain to SD.
  // Rolling match for a literal "STOP\n" shutdown command -- a real capture is binary
  // UBX/NMEA and won't spell this out, so false positives are vanishingly unlikely in bench
  // use. This is a Phase 1 harness convenience, not the eventual status-link protocol
  // (still undecided) that will trigger shutdown in the field.
  static const char kStopCmd[] = "STOP\n";
  static uint8_t stopMatchPos = 0;
  while (Serial.available()) {
    uint8_t b = Serial.read();
    ubx.feed(b);

    if (b == (uint8_t)kStopCmd[stopMatchPos]) {
      stopMatchPos++;
      if (stopMatchPos == sizeof(kStopCmd) - 1) {
        closeLogSafely();
        stopMatchPos = 0;
      }
    } else {
      stopMatchPos = (b == (uint8_t)kStopCmd[0]) ? 1 : 0;
    }
  }

  flushRingToSd();

  static uint32_t lastBlink = 0;
  uint32_t now = millis();
  if (now - lastBlink >= 500) {
    lastBlink = now;
    digitalWrite(PIN_LED_GREEN, sdOk ? !digitalRead(PIN_LED_GREEN) : LOW);
  }

  if (now - lastStatusMs >= 2000) {
    lastStatusMs = now;
    // Same 80-byte printf-buffer reason as closeLogSafely(): keep each call short.
    Serial.printf("[ubx] frames=%lu ckErr=%lu drop=%lu written=%lu\n",
                  (unsigned long)ubx.framesExtracted(), (unsigned long)ubx.checksumErrors(),
                  (unsigned long)ubx.framesDropped(), (unsigned long)bytesWritten);
    unsigned pct = (unsigned)(100u * ubx.ringHighWater() / UbxExtractor::kRingSize);
    Serial.printf("[ubx] ring hwm=%u/%u (%u%%)\n", (unsigned)ubx.ringHighWater(),
                  (unsigned)UbxExtractor::kRingSize, pct);
  }
}
