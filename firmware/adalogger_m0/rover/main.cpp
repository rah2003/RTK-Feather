// M0 Adalogger Rover firmware -- Topology B passive tap logger (docs/hardware/topology.md,
// docs/hardware/wiring.md wire 3): this MCU never drives F9P UART1 RX (wire 3's "same wire,
// parallel tap" -- Serial1 TX/D1 here is firmware-idle and physically unconnected per
// wiring.md's NEVER-connect #1 / checklists.md's SERCOM plan). It only listens on Serial1 RX
// (D0), extracts UBX frames with common/ubx_extractor.h (the exact logic proven in the Phase
// 1 bring-up harness, just fed from the real tap instead of a USB replay), and writes them to
// SD. File naming and the button/LED scheme follow docs/field-guide.md's already-drafted
// convention (draft/pending approval, but consistent with what this firmware implements).
//
// Single-core superloop (project brief §4) -- no FreeRTOS here, this is the stock SAMD21
// Arduino core.
#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>

#include "../../common/status_link.h"
#include "../../common/ubx_extractor.h"
#include "../pins.h"
#include "../serial2_link.h"

SdFat32 sd;
File32 logFile;
UbxExtractor ubx;
LineAssembler cmdIn;

namespace {

bool sdOk = false;
bool loggingEnabled = true;  // "always-on" default (Metro Q6 pattern); B-long toggles it
bool fileOpen = false;
char currentFileName[40] = {0};
uint32_t bytesWritten = 0;
uint32_t totalBytesWritten = 0;  // across all files this session, for the free-space estimate

// Pre-allocate a contiguous region at file open (project brief §3.4 "pre-allocated/contiguous
// file if feasible"): contiguous writes skip most FAT housekeeping, shrinking the write-stall
// spikes the UART RX buffer has to ride out. ~1.6 h at the ~5.6 kB/s worst-case link rate;
// overrunning it just falls back to normal (non-contiguous) growth. Trimmed at close.
constexpr uint32_t kPreAllocBytes = 32UL * 1024 * 1024;

// Clock: TSYNC gives an absolute GNSS time every 60 s; between syncs the clock advances from
// millis() so filenames stay unique and monotonic (review finding: a frozen clock + O_TRUNC
// destroyed the previous file when logging was restarted within one sync window).
bool timeValid = false;
uint16_t curYear = 0;
uint8_t curMonth = 0, curDay = 0;
uint32_t baseSecOfDay = 0;  // seconds-of-day at the last TSYNC
uint32_t baseSyncMs = 0;    // millis() at the last TSYNC

uint32_t cachedFreeKB = 0;  // measured once at boot (full FAT scan); decremented as we write

uint32_t lastFlushMs = 0;
uint32_t lastStatusSendMs = 0;
uint32_t lastBlinkMs = 0;

void nowHms(uint8_t& hh, uint8_t& mm, uint8_t& ss, uint8_t& dayOut) {
  uint32_t sod = baseSecOfDay + (millis() - baseSyncMs) / 1000;
  // Day rollover between syncs: bump the day number arithmetically. Within 60 s of a real
  // rollover this can produce an out-of-range day (e.g. the 32nd) in a filename -- cosmetic
  // and self-correcting at the next TSYNC; uniqueness is what matters here.
  dayOut = curDay + (uint8_t)(sod / 86400);
  sod %= 86400;
  hh = sod / 3600;
  mm = (sod % 3600) / 60;
  ss = sod % 60;
}

bool makeName(char* out, size_t outLen) {
  if (!timeValid) return false;
  uint8_t hh, mm, ss, day;
  nowHms(hh, mm, ss, day);
  char dir[16];
  snprintf(dir, sizeof(dir), "/%04u%02u%02u", curYear, curMonth, day);
  if (!sd.exists(dir)) sd.mkdir(dir);
  // "r_" prefix per docs/field-guide.md (rover); Base mode ("b_") isn't built in v1
  // (FEATURE_BASE=0 on the HUZZAH32 side -- there's no base-mode signal to distinguish here).
  snprintf(out, outLen, "%s/r_%02u%02u%02u.ubx", dir, hh, mm, ss);
  return true;
}

void closeFile() {
  if (!fileOpen) return;
  logFile.truncate(bytesWritten);  // trim the unused tail of the pre-allocation
  logFile.sync();
  logFile.close();
  fileOpen = false;
  Serial.printf("[sd] closed %s (%lu bytes)\n", currentFileName, (unsigned long)bytesWritten);
}

bool openFile() {
  char name[44];
  if (!makeName(name, sizeof(name))) return false;  // no valid time yet
  // Collision guard (belt to the ticking clock's suspenders): never O_TRUNC an existing log.
  // Suffix letters break 8.3 but SdFat builds with long-file-name support by default.
  if (sd.exists(name)) {
    size_t len = strlen(name);
    char c;
    for (c = 'a'; c <= 'z'; c++) {
      snprintf(name + len - 4, 6, "%c.ubx", c);
      if (!sd.exists(name)) break;
    }
    if (c > 'z') {
      Serial.println(F("[sd] 27 filename collisions -- refusing to overwrite; not logging"));
      return false;
    }
  }
  if (!logFile.open(name, O_RDWR | O_CREAT | O_TRUNC)) {
    Serial.printf("[sd] failed to open %s\n", name);
    return false;
  }
  if (!logFile.preAllocate(kPreAllocBytes))
    Serial.println(F("[sd] preAllocate failed (card nearly full?) -- logging non-contiguous"));
  strncpy(currentFileName, name, sizeof(currentFileName) - 1);
  currentFileName[sizeof(currentFileName) - 1] = '\0';
  bytesWritten = 0;
  fileOpen = true;
  Serial.printf("[sd] logging to %s\n", name);
  return true;
}

void flushUbxRingToSd() {
  // AT MOST ONE 512 B chunk per loop pass (review finding): logFile.write() is the only call
  // here that can stall for an SD write spike, and while it's stalled only the SERCOM UART
  // RX buffer absorbs the tap stream. One bounded write per pass keeps Serial1 serviced
  // between chunks; the drain rate (512 B x kHz-loop) still dwarfs the ~5.6 kB/s input.
  static uint8_t chunk[512];
  size_t n = ubx.drain(chunk, sizeof(chunk));
  if (n == 0) return;
  if (fileOpen) {
    logFile.write(chunk, n);
    bytesWritten += n;
    totalBytesWritten += n;
  }
}

uint32_t sdFreeKB() {
  // Estimate only, for the OLED status page: measured once at boot (sd.freeClusterCount()
  // is a FULL FAT SCAN -- hundreds of ms to seconds on a big card; review finding: calling
  // it at 1 Hz during logging starved the tap), then decremented arithmetically as we write.
  // Ignores the transient pre-allocation (reclaimed by truncate() at close).
  uint32_t writtenKB = totalBytesWritten / 1024;
  return cachedFreeKB > writtenKB ? cachedFreeKB - writtenKB : 0;
}

void handleStatusLinkLine(const char* line) {
  TimeSyncMsg ts;
  bool logStart;
  if (parseTimeSync(line, ts)) {
    timeValid = true;
    curYear = ts.year;
    curMonth = ts.month;
    curDay = ts.day;
    baseSecOfDay = ts.hour * 3600UL + ts.minute * 60UL + ts.second;
    baseSyncMs = millis();
    return;
  }
  if (parseLogCtl(line, logStart)) {
    loggingEnabled = logStart;
    Serial.printf("[link] LOGCTL %s\n", logStart ? "START" : "STOP");
    if (!logStart) closeFile();  // reopens fresh on the next START, per makeName()'s new timestamp
    return;
  }
  if (parseShutdown(line)) {
    Serial.println(F("[link] SHUTDOWN -- closing files"));
    closeFile();
    loggingEnabled = false;  // stay closed until reset -- matches "safe to power off"
    return;
  }
  // Unrecognized/bad-checksum line: ignored, matches common/status_link.h's documented
  // failure mode (the HUZZAH32 will just send its next line).
}

void sendStatus() {
  char line[LineAssembler::kMaxLine];
  size_t n = buildM0Status(line, sizeof(line), fileOpen, currentFileName, bytesWritten,
                            sdFreeKB());
  Serial2.write((const uint8_t*)line, n);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 3000) {
  }

  Serial.println(F("=== RTK-Feather M0 Adalogger Rover ==="));

  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  pinMode(PIN_SD_CD, INPUT_PULLUP);

  sdOk = sd.begin(PIN_SD_CS, SD_SCK_MHZ(25));
  digitalWrite(PIN_LED_RED, sdOk ? LOW : HIGH);  // solid red = SD problem (field-guide.md)
  if (!sdOk) {
    Serial.println(F("[sd] init FAILED at CS=D4 -- logging disabled, status link still runs"));
  } else {
    Serial.println(F("[sd] init OK"));
    // One-time full FAT scan for the free-space figure, done HERE -- before Serial1 opens and
    // before logging can start -- because it can take seconds on a big card and must never
    // run inside the logging loop (see sdFreeKB()).
    int32_t freeClusters = sd.freeClusterCount();
    if (freeClusters > 0) {
      cachedFreeKB =
          (uint32_t)(((uint64_t)freeClusters * sd.sectorsPerCluster() * 512ULL) / 1024ULL);
      Serial.printf("[sd] %lu KB free\n", (unsigned long)cachedFreeKB);
    }
  }

  // Passive tap: RX only (D0). D1 (Serial1 TX) is firmware-idle and physically unconnected
  // (docs/hardware/checklists.md SERCOM plan, docs/hardware/wiring.md NEVER-connect #1) --
  // this firmware never calls Serial1.write()/print() and never will.
  // RX buffer is 1024 B via -DSERIAL_BUFFER_SIZE (platformio.ini) -- the core's 350 B default
  // only rides out ~30 ms of the 11,520 B/s tap stream during an SD write stall.
  Serial1.begin(115200);

  serial2Begin(38400);  // status link to the HUZZAH32 (docs/hardware/wiring.md wires 5/6)

  Serial.println(F("[main] waiting for time sync from the HUZZAH32 before logging starts"));
}

void loop() {
  // 1) Passive UBX extraction from the F9P tap.
  while (Serial1.available()) {
    ubx.feed((uint8_t)Serial1.read());
  }

  // 2) Status link: receive commands from the HUZZAH32.
  char line[LineAssembler::kMaxLine];
  while (Serial2.available()) {
    if (cmdIn.feed((uint8_t)Serial2.read(), line, sizeof(line))) {
      handleStatusLinkLine(line);
    }
  }

  // 3) Open the log once we have valid time and logging is enabled, unless the user (via
  //    button on the HUZZAH32) or a shutdown request closed it.
  if (sdOk && loggingEnabled && timeValid && !fileOpen) {
    openFile();
  }

  // 4) Drain extracted UBX frames to SD.
  flushUbxRingToSd();

  // 5) Bounded loss: flush every 8 s so a yanked battery costs at most 8 s of observations
  //    (matches Metro's sd_logger.cpp rationale).
  uint32_t now = millis();
  if (fileOpen && now - lastFlushMs > 8000) {
    lastFlushMs = now;
    logFile.sync();
  }

  // 6) Status back to the HUZZAH32, ~1 Hz.
  if (now - lastStatusSendMs >= 1000) {
    lastStatusSendMs = now;
    sendStatus();
  }

  // 7) LEDs: green heartbeat while actively logging, off when idle (field-guide.md).
  if (now - lastBlinkMs >= 500) {
    lastBlinkMs = now;
    digitalWrite(PIN_LED_GREEN, fileOpen ? !digitalRead(PIN_LED_GREEN) : LOW);
  }
}
