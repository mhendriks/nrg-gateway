#include "Storage.h"
#include "Debug.h"
#include <FS.h>
#include <LittleFS.h>

namespace Storage {
  void begin() {
    if (!LittleFS.begin()) {
      Debug::println("[FS] LittleFS mount failed");
      return;
    }
    Debug::println("[FS] LittleFS OK");
    // TODO: ring-file init / check / self-heal
  }

  void loop() {
    // TODO: periodic flush/batch writes
  }

  void writeHour() {}
  void writeDay() {}
  void writeMonth() {}

  void log(const char* tag, const char* msg) {
    File f = LittleFS.open("/log.txt", FILE_APPEND);
    if (!f) return;
    f.printf("[%lu] %s: %s\n", millis(), tag, msg);
    f.close();
    // TODO: rollover to /log.old when exceeding size
  }
}
