// Button UI. Matches the gesture scheme already drafted (independently, before this file
// was written) in docs/field-guide.md -- itself the project brief §9 proposal, adapted from
// its generic 2-button short/long/hold-both pattern onto this build's actual 3 OLED-Wing
// buttons. field-guide.md still marks it draft/pending approval (QUESTIONS.md) -- flag it if
// you want the mapping changed:
//
//   A (short-press):        cycle OLED page. Tracked even though display.cpp is a stub (the
//                            OLED variant is still unresolved -- QUESTIONS.md Q2) -- nothing
//                            renders yet, but the page counter is ready for when it does.
//   B (long-press >=1.5s):  toggle logging start/stop, relayed to the M0 over the status
//                            link. Toggles off the M0's LAST REPORTED state (not a locally
//                            guessed one), since this protocol has no ACK and a dropped
//                            command shouldn't let the button's idea of "on/off" drift from
//                            the M0's actual state.
//   A+C held together
//   (>=3s):                 safe shutdown request. Deliberately not A+B or B+C: A is the
//                            MTDO strapping pin (docs/hardware/checklists.md) -- fine to hold
//                            at *runtime*, just kept off the highest-consequence gesture
//                            since it already has a hardware caveat attached to it.
#include "ui_task.h"

#include "pins.h"
#include "shared.h"
#include "status_link_task.h"

namespace {

constexpr uint32_t kLongPressMs = 1500;
constexpr uint32_t kShutdownHoldMs = 3000;  // matches docs/field-guide.md's already-drafted "A+C hold >=3s"
constexpr uint32_t kDebounceMs = 30;

struct ButtonState {
  bool pressed = false;  // raw, debounced
  bool stable = false;
  uint32_t lastChangeMs = 0;
  uint32_t pressedAtMs = 0;
  bool longFired = false;  // a long/combo gesture already fired for this press
};

ButtonState btnA, btnB, btnC;
int currentPage = 0;
constexpr int kNumPages = 4;  // fix / NTRIP / logging / BLE -- project brief §3.5

void poll(ButtonState& b, int pin) {
  bool raw = digitalRead(pin) == LOW;  // all three read LOW when pressed
  uint32_t now = millis();
  if (raw != b.pressed) {
    b.pressed = raw;
    b.lastChangeMs = now;
  }
  if (now - b.lastChangeMs >= kDebounceMs && b.stable != b.pressed) {
    b.stable = b.pressed;
    if (b.stable) {
      b.pressedAtMs = now;
      b.longFired = false;
    }
  }
}

void uiTask(void*) {
  pinMode(PIN_BUTTON_A, INPUT_PULLUP);
  pinMode(PIN_BUTTON_B, INPUT_PULLUP);
  pinMode(PIN_BUTTON_C, INPUT_PULLUP);

  bool prevA = false;

  for (;;) {
    poll(btnA, PIN_BUTTON_A);
    poll(btnB, PIN_BUTTON_B);
    poll(btnC, PIN_BUTTON_C);
    uint32_t now = millis();

    if (btnA.stable && btnC.stable) {
      // Shutdown combo: fires once per co-held press, timed from whichever of the two was
      // pressed last (so it always means "both have now been down this long").
      uint32_t heldSince = max(btnA.pressedAtMs, btnC.pressedAtMs);
      if (!btnA.longFired && !btnC.longFired && now - heldSince >= kShutdownHoldMs) {
        btnA.longFired = btnC.longFired = true;
        Serial.println(F("[ui] A+C held -- safe shutdown requested"));
        statusLinkRequestShutdown();
      }
    } else if (btnB.stable && !btnB.longFired && now - btnB.pressedAtMs >= kLongPressMs) {
      btnB.longFired = true;
      M0Status m0 = statusGetM0();
      bool requestStart = !m0.logging;
      Serial.printf("[ui] B long-press -- logging %s requested\n",
                    requestStart ? "START" : "STOP");
      statusLinkRequestLogCtl(requestStart);
    }

    // A short-press (released before its long-press/combo threshold fired): cycle page.
    if (prevA && !btnA.stable && !btnA.longFired) {
      currentPage = (currentPage + 1) % kNumPages;
      Serial.printf("[ui] page -> %d\n", currentPage);
    }
    prevA = btnA.stable;

    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

}  // namespace

void uiTaskStart() {
  xTaskCreatePinnedToCore(uiTask, "ui", 3072, nullptr, 1, nullptr, 1 /* core 1 */);
}
