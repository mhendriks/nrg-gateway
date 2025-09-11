#pragma once

namespace Debug {
  inline void begin(uint32_t baud=115200) { Serial.begin(baud); while(!Serial) { delay(1);} }
  inline void print(const char* s) { Serial.print(s); }
  inline void println(const char* s) { Serial.println(s); }
  inline void println() { Serial.println(); }
  template<typename... Args>
  inline void printf(const char* fmt, Args... args) { Serial.printf(fmt, args...); }
}
