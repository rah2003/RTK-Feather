// Status link to the M0 Adalogger -- new for this project (Metro is a single MCU and has no
// equivalent). Protocol lives in common/status_link.h so both sides share the exact same
// wire format and checksum code; this file is just the HUZZAH32-side transport (Serial2)
// and scheduling around it.
#include "status_link_task.h"

#include <esp_task_wdt.h>

#include "../../common/status_link.h"
#include "pins.h"
#include "shared.h"

namespace {

volatile bool logCtlRequested = false;
volatile bool logCtlStartValue = false;
volatile bool shutdownRequested = false;

LineAssembler lineIn;

// Resend period for time sync once already valid -- corrects the M0's software clock drift
// over a long session. The first send happens as soon as GnssStatus.timeValid goes true,
// independent of this timer.
constexpr uint32_t kTimeSyncPeriodMs = 60000;

void sendLine(const char* line, size_t len) {
  if (len) Serial2.write((const uint8_t*)line, len);
}

void statusLinkTask(void*) {
  esp_task_wdt_add(nullptr);
  Serial2.begin(38400, SERIAL_8N1, PIN_STATUS_RX, PIN_STATUS_TX);

  bool timeSyncedOnce = false;
  uint32_t lastTimeSyncMs = 0;
  char lineBuf[LineAssembler::kMaxLine];
  char outBuf[64];

  for (;;) {
    esp_task_wdt_reset();

    // Receive: drain whatever the M0 has sent, parse complete lines as MSTA.
    while (Serial2.available()) {
      uint8_t b = Serial2.read();
      if (lineIn.feed(b, lineBuf, sizeof(lineBuf))) {
        M0StatusMsg msg;
        if (parseM0Status(lineBuf, msg)) {
          M0Status s;
          s.linkUp = true;
          s.logging = msg.logging;
          strncpy(s.fileName, msg.fileName, sizeof(s.fileName) - 1);
          s.fileName[sizeof(s.fileName) - 1] = '\0';
          s.bytesWritten = msg.bytesWritten;
          s.sdFreeKB = msg.sdFreeKB;
          s.lastMsgMs = millis();
          statusSetM0(s);
        }
        // Anything else on the link (malformed line, wrong checksum) is silently dropped --
        // the M0 will just send its next status line ~1 s later.
      }
    }

    // Send: time sync, once time is valid and then periodically for drift correction.
    GnssStatus gs = statusGetGnss();
    if (gs.timeValid) {
      bool due = !timeSyncedOnce || (millis() - lastTimeSyncMs >= kTimeSyncPeriodMs);
      if (due) {
        size_t n = buildTimeSync(outBuf, sizeof(outBuf), gs.year, gs.month, gs.day, gs.hour,
                                  gs.minute, gs.second);
        sendLine(outBuf, n);
        timeSyncedOnce = true;
        lastTimeSyncMs = millis();
      }
    }

    // Send: button-requested log start/stop.
    if (logCtlRequested) {
      logCtlRequested = false;
      size_t n = buildLogCtl(outBuf, sizeof(outBuf), logCtlStartValue);
      sendLine(outBuf, n);
    }

    // Send: safe shutdown. Sent a few times with short gaps -- this protocol has no ACK, and
    // "safe to power off" is exactly the moment we most want delivery to not depend on luck.
    if (shutdownRequested) {
      shutdownRequested = false;
      size_t n = buildShutdown(outBuf, sizeof(outBuf));
      for (int i = 0; i < 3; i++) {
        sendLine(outBuf, n);
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

}  // namespace

void statusLinkTaskStart() {
  xTaskCreatePinnedToCore(statusLinkTask, "statlink", 4096, nullptr, 1, nullptr, 1 /* core 1 */);
}

void statusLinkRequestLogCtl(bool start) {
  logCtlStartValue = start;
  logCtlRequested = true;
}

void statusLinkRequestShutdown() { shutdownRequested = true; }
