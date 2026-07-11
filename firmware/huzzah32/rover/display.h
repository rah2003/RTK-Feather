#pragma once

// OLED status display -- 128x32 SSD1306 FeatherWing (product #2900, I2C 0x3C fixed;
// QUESTIONS.md Q2 answered 2026-07-11). Four button-cycled pages (project brief §3.5,
// layouts in display.cpp), rendered with Adafruit_SSD1306 + Adafruit_GFX.
//
// 128x32 with the stock 6x8 font is 21 columns x 4 rows -- every page layout in
// display.cpp is designed to that budget (abbreviated labels, basename-only filenames).
// The correction-age >10 s alarm is inverse-video flash (brief §9: color is impossible on
// a mono panel).
//
// If the Wing is absent (I2C 0x3C not found) the firmware runs headless: displayUpdate()
// keeps printing the 2 s status line to serial and skips rendering.
void displayInit();
void displayUpdate();  // call periodically from loop(); self-paces
