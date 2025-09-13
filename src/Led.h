#pragma once
#include <Arduino.h>
#include "Config.h"

namespace Led {
  enum class Mode : uint8_t { OFF=0, ON, PREVIEW_SHORT, PREVIEW_LONG, PREVIEW_VLONG, CONFIRM_SHORT, CONFIRM_LONG, CONFIRM_VLONG };

  void begin();                 // init pins/driver op basis van Config::io
  void loop();                  // non-blocking animaties (blink/preview)
  void set(Mode m);             // zet modus (Button roept dit aan)
  void setOn(bool on);          // hard aan/uit (bijv. voor foutstatus elders)

  // Ultras-only (weak): mag ontbreken in mono builds
  void __attribute__((weak)) SetRGB(uint8_t r, uint8_t g, uint8_t b);
}
