#pragma once
#include <Arduino.h>

#if  ARDUINO_USB_CDC_ON_BOOT
  #define USBSerial Serial
  #define DEBUG
#else 
  extern HWCDC USBSerial;
#endif

namespace Debug {
  inline void begin(uint32_t baud=115200) { Serial.begin(baud); while(!Serial) { delay(1);} }
  inline void print(const char* s) { Serial.print(s); }
  inline void println(const char* s) { Serial.println(s); }
  inline void println() { Serial.println(); }
  
  inline void print(const __FlashStringHelper* s) { Serial.print(s); }
  inline void println(const __FlashStringHelper* s) { Serial.println(s); }

  inline void printf(const char* fmt, ...) {
    va_list ap; va_start(ap, fmt); Serial.vprintf(fmt, ap); va_end(ap);
  }
  template<typename... Args>
  inline void printf(const char* fmt, Args... args) { Serial.printf(fmt, args...); }
  
  inline void usbbegin(uint32_t baud=115200) { USBSerial.begin(baud); }
  template<typename... Args>
  inline void usbprintf(const char* fmt, Args... args) { USBSerial.printf(fmt, args...); }
}
