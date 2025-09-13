#include "Button.h"
#include "Led.h"
#include "Config.h"
#include <WiFi.h>
#include "Network.h"

namespace Button {

static Timings timings;
static volatile bool edgeFlag=false, lastLevel=true;
static volatile uint32_t lastEdgeAt_ms=0;
static uint32_t lastDebounce_ms=0, pressedAt_ms=0;
static bool isHeld=false;
static Kind pending=Kind::NONE, previewMode=Kind::NONE;

static inline int  BTN_PIN()    { return Config::io.btn_pin; }
static inline bool BTN_AL()     { return Config::io.btn_active_low; }

static bool isPressedLevel(bool raw) { return BTN_AL()? (raw==LOW):(raw==HIGH); }

static Kind classify(uint32_t d){
  if (d < timings.short_ms) return Kind::SHORT;
  if (d >= timings.very_long_ms) return Kind::VERY_LONG;
  if (d >= timings.long_ms) return Kind::LONG;
  return Kind::SHORT;
}

static void schedule(Kind k){ pending=k; }

static ActionFn onShortCb=nullptr, onLongCb=nullptr, onVeryLongCb=nullptr;

static void IRAM_ATTR isrThunk(void*){
  lastLevel = digitalRead(BTN_PIN());
  lastEdgeAt_ms = millis();
  edgeFlag = true;
}

void begin(){
  // timings uit Config
  timings.debounce_ms       = Config::btn.debounce_ms;
  timings.short_ms          = Config::btn.short_ms;
  timings.long_ms           = Config::btn.long_ms;
  timings.very_long_ms      = Config::btn.very_long_ms;
  timings.preview_period_ms = Config::btn.preview_period_ms;

  Led::begin();

  if (BTN_PIN()>=0){
    pinMode(BTN_PIN(), BTN_AL()?INPUT_PULLUP:INPUT);
    attachInterruptArg(BTN_PIN(), isrThunk, (void*)nullptr, CHANGE);
  }
}

void loop(){
  // consume edge
  noInterrupts();
    bool haveEdge = edgeFlag; edgeFlag=false;
    bool level = lastLevel;
  interrupts();

  const uint32_t now = millis();

  if (haveEdge){
    if (now - lastDebounce_ms >= timings.debounce_ms){
      lastDebounce_ms = now;
      if (isPressedLevel(level)){
        pressedAt_ms = now; isHeld = true;
      } else {
        if (isHeld){
          Kind k = classify(now - pressedAt_ms);
          previewMode = k;
          schedule(k);
          isHeld = false;
          // bevestig kort
          using LM=Led::Mode;
          Led::set( k==Kind::SHORT? LM::CONFIRM_SHORT : k==Kind::LONG? LM::CONFIRM_LONG : LM::CONFIRM_VLONG );
        }
      }
    }
  }

  // live preview tijdens vasthouden
  if (isHeld){
    using LM=Led::Mode;
    Kind k = classify(now - pressedAt_ms);
    Led::set( k==Kind::SHORT? LM::PREVIEW_SHORT : k==Kind::LONG? LM::PREVIEW_LONG : LM::PREVIEW_VLONG );
  } else {
    Led::loop(); // laat animatie uitlopen (confirm -> OFF)
  }

  // pending actie uitvoeren
  if (pending != Kind::NONE){
    switch (pending){
      case Kind::SHORT:     if (onShortCb) onShortCb(); else OnShort(); break;
      case Kind::LONG:      if (onLongCb)  onLongCb();  else OnLong();  break;
      case Kind::VERY_LONG: if (onVeryLongCb) onVeryLongCb(); else OnVeryLong(); break;
      default: break;
    }
    pending = Kind::NONE;
  }
}

void setHandlers(ActionFn s, ActionFn l, ActionFn vl){ onShortCb=s; onLongCb=l; onVeryLongCb=vl; }

// Defaults (weak)
void OnShort(){ delay(50); esp_restart(); }
void OnLong(){ /* zet bv. espnow_pair_request=true; */ }
void OnVeryLong(){ NetworkMgr::instance().forgetWifiCreds(true); }

} // namespace Button
