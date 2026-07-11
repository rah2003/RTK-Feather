#pragma once
#include <Arduino.h>

// Runs on core 0. Owns WiFi association and the NTRIP socket; RTCM bytes go into rtcmRing
// (gnssTask injects them into the F9P on core 1 -- this task never touches Serial1).
void ntripTaskStart();

const char* ntripStateName();  // for the serial menu / status line
