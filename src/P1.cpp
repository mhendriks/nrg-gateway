#include "P1.h"

// Stub values
static float s_power=0, s_t1=0, s_t2=0, s_t1r=0, s_t2r=0, s_gas=0, s_water=0, s_solar=0;

namespace P1 {
  void begin() {
    // TODO: init DSMR parser, Serial pins/baud dependent on meter
  }

  void loopHighPrio() {
    // TODO: parse telegrams, update s_* variables
  }

  float powerW(){ return s_power; }
  float t1kWh(){ return s_t1; }
  float t2kWh(){ return s_t2; }
  float t1rKWh(){ return s_t1r; }
  float t2rKWh(){ return s_t2r; }
  float gasM3(){ return s_gas; }
  float waterL(){ return s_water; }
  float solarW(){ return s_solar; }
}
