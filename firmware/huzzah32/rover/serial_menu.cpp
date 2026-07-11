#include "serial_menu.h"

#include "gnss_task.h"
#include "ntrip_client.h"
#include "settings.h"
#include "shared.h"

namespace {

char lineBuf[160];
size_t lineLen = 0;

void printHelp() {
  Serial.println(F("--- RTK-Feather serial menu ---"));
  Serial.println(F("key=value      set + save a setting: wifi1/pass1..wifi4/pass4, caster,"));
  Serial.println(F("               port, mount, user, password, ggaperiod, elevmask, logubx"));
  Serial.println(F("status         settings + live status"));
  Serial.println(F("save           persist current settings to NVS"));
  Serial.println(F("factoryrecover F9P UBX-CFG-CFG reset + reapply project config"));
  Serial.println(F("help           this text"));
}

void printStatus() {
  settingsPrint(Serial);
  GnssStatus gs = statusGetGnss();
  M0Status m0 = statusGetM0();
  Serial.printf("f9p=%s fix=%u carr=%u sv=%u lat=%.7f lon=%.7f\n", gs.f9pDetected ? "yes" : "no",
                gs.fixType, gs.carrSoln, gs.numSV, gs.latDeg, gs.lonDeg);
  Serial.printf("ntrip=%s wifi=%s\n", ntripStateName(), g_link.wifiUp ? "up" : "down");
  Serial.printf("m0 link=%s logging=%s file=%s bytes=%lu sdFreeKB=%lu\n",
                m0.linkUp ? "up" : "down", m0.logging ? "on" : "off",
                m0.fileName[0] ? m0.fileName : "-", (unsigned long)m0.bytesWritten,
                (unsigned long)m0.sdFreeKB);
}

void handleLine(char* line) {
  if (!line[0]) return;
  if (!strcmp(line, "help")) { printHelp(); return; }
  if (!strcmp(line, "status")) { printStatus(); return; }
  if (!strcmp(line, "save")) { settingsSave(); Serial.println(F("saved")); return; }
  if (!strcmp(line, "factoryrecover")) {
    gnssRequestFactoryRecover();
    Serial.println(F("requested"));
    return;
  }
  char* eq = strchr(line, '=');
  if (!eq) {
    Serial.println(F("unrecognized -- 'help' for commands"));
    return;
  }
  *eq = '\0';
  char* val = eq + 1;
  if (settingsApplyKeyValue(line, val)) {
    settingsSave();
    Serial.println(F("ok, saved"));
  } else {
    Serial.println(F("unknown key -- 'help' for the list"));
  }
}

}  // namespace

void serialMenuPoll() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        handleLine(lineBuf);
        lineLen = 0;
      }
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}
