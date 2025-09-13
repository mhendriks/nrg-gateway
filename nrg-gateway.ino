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
#include "src/MQTT.h"
#include "src/Storage.h"
#include "src/Led.h"
#include "src/Button.h"
#include "src/OTAfw.h"
#include "src/EspNow.h"
#include "src/Web.h"
#include "esp_task_wdt.h"

void SetupWDT(){
  esp_task_wdt_deinit();
  esp_task_wdt_config_t cfg = {
    .timeout_ms = 15000, //in 15sec default 
    // .idle_core_mask = (1<<0) | (1<<1), //S3 watch idle core 0 & 1
    // .idle_core_mask = (1<<0), //C3 watch idle core 0 
    .idle_core_mask = 0, //idle core watch dog OFF
#ifdef DEBUG
    .trigger_panic = true
#else
    .trigger_panic = false
#endif
  };
  esp_task_wdt_init(&cfg);
  esp_task_wdt_add(NULL);
}

void setup() {
  Debug::begin(115200);
  Debug::usbbegin(115200);
  Debug::usbprintf("NRG Gateway Firmware | %s | %s - %s %s \n", Config::hostName(), FW_VERSION, __DATE__, __TIME__);
  SetupWDT();
  Led::begin();
  // StatusLed::begin();
  Storage::begin();
  Config::load();                 // load NVS / defaults
  NetworkMgr::instance().setup(NetworkProfile::Ultra /* of WiFiOnly/EthOnly */);
  P1::begin();                    // DSMR parser (stub)
  Web::begin();                   // HTTP + WebSockets + RAW:82
  // MQTT::begin();                  // connect broker, publish LWT online
  Button::begin();                // ISR-safe, actions via queue
  // EspNow::maybeBegin();           // optional
  // OTAfw::begin();              // optional hooks
}

void QueueLoop(){
  //todo in worker
  Web::rawPrint();
}

void loop() {
  // MQTT::loop();
  Web::loop();
  Storage::loop();
  Led::loop();
  QueueLoop();
  // OTAfw::loop();
  // StatusLed::loop();
  Button::loop();
  NetworkMgr::instance().tick();
  esp_task_wdt_reset();
}