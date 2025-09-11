#pragma once
#include <Arduino.h>

namespace P1 {
  void begin();
  void loopHighPrio();

  // Example getters used by Web/MQTT
  float powerW();
  float t1kWh(); float t2kWh(); float t1rKWh(); float t2rKWh();
  float gasM3(); float waterL();
  float solarW();
}
