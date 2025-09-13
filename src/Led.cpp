#include "Config.h"
#include "Led.h"

namespace Led {

static Led::Mode mode = Led::Mode::OFF;
static uint32_t t0=0;

static inline int  LED_PIN()      { return Config::io.led_pin; }
static inline bool LED_AH()       { return Config::io.led_active_high; }
static inline bool LED_IS_RGB()   { return Config::io.led_is_rgb; }

static void writeMono(bool on) {
  if (LED_PIN() < 0) return;
  digitalWrite(LED_PIN(), (on==LED_AH())?HIGH:LOW);
}

void begin() {
  if (LED_PIN() >= 0) { pinMode(LED_PIN(), OUTPUT); writeMono(false); }
  if (LED_IS_RGB() && (&SetRGB != nullptr)) SetRGB(0,0,0);
  mode = Mode::OFF; t0 = millis();
}

void setOn(bool on) {
  mode = on ? Mode::ON : Mode::OFF;
  if (LED_IS_RGB() && (&SetRGB != nullptr)) SetRGB(on?255:0, on?255:0, on?255:0);
  else writeMono(on);
}

void set(Mode m) { mode = m; t0 = millis(); }

void loop() {
  const uint32_t now = millis();
  // Mono patterns: preview 1/2/3 korte pulses per ~800ms, confirm korte burst ~400ms
  const uint32_t T = Config::btn.preview_period_ms ? Config::btn.preview_period_ms : 800;
  const uint32_t onw=120, gap=80;

  auto pulsesFor = [](Mode m)->uint8_t {
    if (m==Mode::PREVIEW_SHORT || m==Mode::CONFIRM_SHORT) return 1;
    if (m==Mode::PREVIEW_LONG  || m==Mode::CONFIRM_LONG ) return 2;
    if (m==Mode::PREVIEW_VLONG || m==Mode::CONFIRM_VLONG) return 3;
    return 0;
  };

  switch (mode) {
    case Mode::OFF: writeMono(false); if (LED_IS_RGB() && (&SetRGB != nullptr)) SetRGB(0,0,0); break;
    case Mode::ON:  writeMono(true);  if (LED_IS_RGB() && (&SetRGB != nullptr)) SetRGB(255,255,255); break;

    case Mode::PREVIEW_SHORT:
    case Mode::PREVIEW_LONG:
    case Mode::PREVIEW_VLONG: {
      if (LED_IS_RGB() && (&SetRGB != nullptr)) {
        // Ultras: vaste kleur voor herkenning tijdens vasthouden
        if (mode==Mode::PREVIEW_SHORT) SetRGB(255,255,255);
        else if (mode==Mode::PREVIEW_LONG) SetRGB(0,255,255);
        else SetRGB(255,0,0);
        break;
      }
      const uint8_t pulses = pulsesFor(mode);
      const uint32_t t = now % T;
      bool on=false;
      for (uint8_t i=0;i<pulses;i++) {
        uint32_t start = i*(onw+gap);
        if (t>=start && t<start+onw) { on=true; break; }
      }
      writeMono(on);
    } break;

    case Mode::CONFIRM_SHORT:
    case Mode::CONFIRM_LONG:
    case Mode::CONFIRM_VLONG: {
      if (LED_IS_RGB() && (&SetRGB != nullptr)) {
        if (mode==Mode::CONFIRM_SHORT) SetRGB(255,255,255);
        else if (mode==Mode::CONFIRM_LONG) SetRGB(0,255,255);
        else SetRGB(255,0,0);
        // korte confirm ~400ms
        if (now - t0 > 400) { mode=Mode::OFF; }
        break;
      }
      const uint8_t pulses = pulsesFor(mode);
      const uint32_t tick=120;
      const uint32_t seq = (now / tick) % (2*pulses);
      writeMono( (seq%2)==0 );
      if (now - t0 > 400) { mode = Mode::OFF; }
    } break;
  }
}

void SetRGB(uint8_t r, uint8_t g, uint8_t b) {
  rgbLedWrite(Config::io.led_pin,r,g,b);
}

} // namespace Led
