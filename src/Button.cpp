#include "Button.h"

#ifndef BUTTON_PIN
#define BUTTON_PIN 0
#endif

static uint8_t last=HIGH;
static uint32_t pressedAt=0;

namespace Button {
  void begin() {
    pinMode(BUTTON_PIN, INPUT_PULLUP);
  }
  void loop() {
    uint8_t cur = digitalRead(BUTTON_PIN);
    if (cur==LOW && last==HIGH) {
      pressedAt = millis();
    } else if (cur==HIGH && last==LOW) {
      uint32_t dur = millis() - pressedAt;
      if (dur > 5000) {
        ESP.restart(); // very long press → reboot placeholder
      }
    }
    last = cur;
  }
}
