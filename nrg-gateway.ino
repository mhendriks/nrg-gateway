/*
  NRG Gateway Firmware (v6) - Skeleton
  ------------------------------------
  Arduino IDE project skeleton with modular file layout.
  - AsyncWebServer + WebSockets
  - MQTT topics (nrg/<device>/...)
  - Storage (ring-files stubs)
  - P1 parser hooks (stubs)
  - Status LED + Button (stubs)
  - OTA (stubs)
  - ESP-NOW (stubs)

  NOTE: This is a buildable starting point if dependencies are installed.
*/

#include "src/Config.h"
#include "src/Debug.h"
#include "src/version.h"

#include "src/Network.h"
#include "src/P1.h"
#include "src/Web.h"
#include "src/MQTT.h"
#include "src/Storage.h"
#include "src/StatusLed.h"
#include "src/Button.h"
#include "src/OTAfw.h"
#include "src/EspNow.h"

void setup() {
  Debug::begin(115200);
  Debug::usbbegin(115200);
  Debug::usbprintf("NRG Gateway Firmware | %s | %s - %s %s \n", Config::hostName(), FW_VERSION, __DATE__, __TIME__);

  StatusLed::begin();
  Storage::begin();
  Config::load();                 // load NVS / defaults
  Networks::begin();               // Wi-Fi / Ethernet + NTP
  P1::begin();                    // DSMR parser (stub)
  Web::begin();                   // HTTP + WebSockets + RAW:82
  MQTT::begin();                  // connect broker, publish LWT online
  Button::begin();                // ISR-safe, actions via queue
  EspNow::maybeBegin();           // optional
  OTAfw::begin();                 // optional hooks
}

void loop() {
  MQTT::loop();
  Web::loop();
  Storage::loop();
  OTAfw::loop();
  StatusLed::loop();
  Button::loop();
}