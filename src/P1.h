#pragma once
#include <Arduino.h>

namespace P1 {
  using RawSink = void (*)(const char*, size_t);

  // Procesbeheer
  void begin();           // start FreeRTOS-task
  void stop();            // (optioneel) stop task
  bool running();         // draait de task?

  // Status
  bool   isV5();
  bool   hasFix();
  bool   offline();
  uint32_t lastTelegramMs();
  uint32_t intervalMs();
  const char* versionStr();

  // Live (uit MyData)
  float powerW();
  float t1kWh(); 
  float t2kWh();
  float t1rKWh(); float t2rKWh();
  float gasM3();  float waterL();
  float solarW();

  // Hooks
  void onRaw(RawSink cb);
  void setVirtual(bool en);
}
