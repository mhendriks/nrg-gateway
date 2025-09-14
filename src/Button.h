/*

TODO: 
- toevoegen van long_ms 

*/

#pragma once
#include <Arduino.h>

namespace Button {

struct Timings {
  uint32_t debounce_ms       = 40;
  uint32_t short_ms          = 500;   // < short => SHORT
  uint32_t long_ms           = 2000;  // [long..very_long) => LONG
  uint32_t very_long_ms      = 6000;  // >= very_long => VERY_LONG
  uint32_t preview_period_ms = 800;   // LED preview periode
};

enum class Kind : uint8_t { NONE=0, SHORT, LONG, VERY_LONG };

// ---------- Publieke API (compatibel) ----------
void begin();      // init pins + ISR, timings uit config
void loop();       // non-blocking afhandeling

// ---------- Optioneel: eigen handlers instellen ----------
using ActionFn = void(*)();
void setHandlers(ActionFn onShort, ActionFn onLong, ActionFn onVeryLong);

// ---------- Weak hooks (kun je project-breed overriden) ----------
void __attribute__((weak)) OnShort();      // default: reboot
void __attribute__((weak)) OnLong();       // default: ESP-NOW pairing placeholder
void __attribute__((weak)) OnVeryLong();   // default: WiFi erase + reboot

// Ultra (RGB) optional hook
// void LedSetRGB(uint8_t r, uint8_t g, uint8_t b);

} // namespace Button
