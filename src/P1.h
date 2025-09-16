/*

TODO: 
- eigen reader of toch de proven dsmr reader gebruiken -> proven
- detect once 
- post processing na lezing 
- uitgaande p1 aansturen

*/

#pragma once
#include <Arduino.h>
#include "esp_task_wdt.h"
#include <ArduinoJson.h>


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
  bool    newTelegram();
  void   clearNewTelegram();
  uint32_t lastTelegramMs();
  uint32_t intervalMs();
  const char* versionStr();

  // Live (uit MyData)
  float powerW();
  float powerkW();
  float t1kWh(); 
  float t2kWh();
  float t1rKWh(); float t2rKWh();
  float gasM3();  float waterL();
  float solarW();

  extern String RawTelegram;

  struct Stats {
    uint32_t StartTime = 0;
    uint32_t U1piek = 0;
    uint32_t U2piek = 0;
    uint32_t U3piek = 0;
    uint32_t U1min = 0xFFFFFFFF;
    uint32_t U2min = 0xFFFFFFFF;
    uint32_t U3min = 0xFFFFFFFF;  
    uint32_t TU1over = 0;
    uint32_t TU2over = 0;
    uint32_t TU3over = 0;
    uint32_t I1piek = 0xFFFFFFFF;
    uint32_t I2piek = 0xFFFFFFFF;
    uint32_t I3piek = 0xFFFFFFFF;
    uint32_t Psluip = 0xFFFFFFFF;
    uint32_t P1max  = 0;
    uint32_t P2max  = 0;
    uint32_t P3max  = 0;
    uint32_t P1min   = 0xFFFFFFFF;
    uint32_t P2min   = 0xFFFFFFFF;
    uint32_t P3min   = 0xFFFFFFFF; 
  };

  extern Stats P1Stats;

  // Hooks
  // inline const Stats& getStats() { return P1Stats; }
  // inline Stats&       mutStats() { return P1Stats; }
  void onRaw(RawSink cb);
  void setVirtual(bool en);
  void broadcastFields();
  void resetStats();
}
