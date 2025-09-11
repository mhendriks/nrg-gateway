#pragma once
#include <Arduino.h>

namespace StatusLed {
  void begin();
  void loop();
  void setError(bool e);
}
