#pragma once
#include <Arduino.h>

// Status-link UART on SERCOM1: D11=PA16=SERCOM1 pad 0=RX, D10=PA18=SERCOM1 pad 2=TX,
// peripheral mux C (PIO_SERCOM, not ALT -- ALT would grab SERCOM3/Wire instead). This is
// the exact recipe from docs/hardware/checklists.md / wiring.md, citing the Adafruit SERCOM
// guide (learn.adafruit.com/using-atsamd21-sercom-to-add-more-spi-i2c-serial-ports).
// SERCOM budget on this variant: 0=Serial1(D0/D1), 3=Wire(D20/D21), 4=SPI/SD, 5=core-hidden
// Serial5 (handler already defined -- never redefine SERCOM5_Handler). Free: SERCOM1 (used
// here), SERCOM2.
extern Uart Serial2;

// Starts Serial2 at `baud` and configures the SERCOM pin mux. Call once from setup().
void serial2Begin(unsigned long baud);
