#pragma once

// Bench debug REPL over USB serial -- how credentials and settings get set without a
// secrets.h (see settings.h header comment: this board has no SD slot, so Metro's
// "/config.txt on SD" mechanism has no card to live on here). "key=value" sets and persists
// a setting via settingsApplyKeyValue(); a few bare commands cover status/save/recovery.
// Call serialMenuPoll() once per loop().
void serialMenuPoll();
