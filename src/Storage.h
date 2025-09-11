#pragma once
#include <Arduino.h>

namespace Storage {
  void begin();   // mount FS, check ring-files, self-heal
  void loop();    // batch writes

  // ring write stubs
  void writeHour();
  void writeDay();
  void writeMonth();

  void log(const char* tag, const char* msg);
}
