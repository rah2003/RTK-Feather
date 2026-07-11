#pragma once

// OLED status display -- STUBBED. docs/QUESTIONS.md Q2 is still open: which FeatherWing
// variant is on hand, 128x32 SSD1306 or 128x64 SH1107? Driver library and page layout differ
// (docs/hardware/checklists.md). Rather than guess, this stub keeps the I2C bus initialized
// and prints the same status a real display would show to the serial console -- swap the
// bodies of displayInit()/displayUpdate() for Adafruit_SSD1306 or Adafruit_SH110X calls once
// Q2 is answered; nothing else in the firmware needs to change (ui_task already tracks the
// current page in currentPage-equivalent state).
void displayInit();
void displayUpdate();  // call periodically from loop(); self-paces
