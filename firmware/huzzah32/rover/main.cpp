// HUZZAH32 Rover firmware -- main entry. Boot order: settings (NVS, optionally seeded from
// secrets.h on first boot) -> status plumbing -> tasks. Task/core layout matches the project
// brief §4: Core 0 = WiFi/NTRIP (+ BLE if FEATURE_BLE=1), Core 1 = UART1 handling
// (gnss_task), status link to the M0, buttons, display.
#include <Arduino.h>
#include <esp_task_wdt.h>

#include "ble_bridge.h"
#include "display.h"
#include "features.h"
#include "gnss_task.h"
#include "ntrip_client.h"
#include "serial_menu.h"
#include "settings.h"
#include "shared.h"
#include "status_link_task.h"
#include "ui_task.h"

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) delay(10);  // wait briefly for a USB console

  Serial.println();
  Serial.println(F("=== RTK-Feather HUZZAH32 Rover ==="));
  Serial.println(F("build: " __DATE__ " " __TIME__));

  // Task watchdog: 30 s, panic->reboot. A hung NTRIP socket or wedged GNSS transaction must
  // reboot the box rather than strand it silent in the field (matches the Metro project's
  // rule -- rah2003/SWMaps-propertylines main.cpp).
  esp_task_wdt_init(30, true);

  settingsLoad();
  settingsPrint(Serial);

  statusInit();
  displayInit();

  gnssTaskStart();        // core 1 -- start first so F9P config lands early
  statusLinkTaskStart();  // core 1
  uiTaskStart();          // core 1

#if FEATURE_BLE
  bleTaskStart();  // core 0
#endif
#if FEATURE_NTRIP
  ntripTaskStart();  // core 0 -- Rover-only build (FEATURE_BASE=0), always runs
#endif

  Serial.println(F("[main] tasks running -- 'help' for the serial menu"));
}

void loop() {
  serialMenuPoll();
  displayUpdate();
  vTaskDelay(pdMS_TO_TICKS(20));
}
