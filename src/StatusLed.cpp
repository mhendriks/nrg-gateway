#include "StatusLed.h"

#ifndef LED_BUILTIN
#define LED_BUILTIN 2
#endif

static bool errorState=false;
static uint32_t t0=0;

namespace StatusLed {
  void begin() {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
  }
  void loop() {
    uint32_t now = millis();
    uint32_t period = errorState ? 200 : 1000;
    if (now - t0 > period) {
      t0 = now;
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
  }
  void setError(bool e){ errorState=e; }
}
