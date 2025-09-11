#pragma once
#include <Arduino.h>

namespace MQTT {
  void begin();
  void loop();

  void publishState();    // nrg/<id>/state
  void publishTele();     // nrg/<id>/tele
  void publishHealth();   // nrg/<id>/health
}
