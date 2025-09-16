/*
TODO: 
- Setup WDT andere plek geven
*/

/*
  NRG Gateway Firmware (aka dsmr-api v6)
  ------------------------------------
  Arduino IDE project with modular layout.
  - AsyncWebServer + WebSockets
  - MQTT topics (nrg/<device>/...)
  - Storage (ring-files stubs)
  - P1 parser hooks (stubs)
  - Status LED + Button (stubs)
  - OTA (stubs)
  - ESP-NOW (stubs)
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
#include "src/WS.h"
#include "src/Raw.h"
#include "esp_task_wdt.h"
#include "src/Time.h"

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
  Button::begin();
  Storage::begin();
  Config::load();                 // load NVS / defaults
  NetworkMgr::instance().setup(NetworkProfile::Ultra /* of WiFiOnly/EthOnly */);
  TimeMgr::begin();
  P1::begin();                    // DSMR parser (stub)
  Web::begin();                   // HTTP + WebSockets + RAW:82

  // MQTT::begin();                  // connect broker, publish LWT online
  // EspNow::maybeBegin();           // optional
  // OTAfw::begin();              // optional hooks

  TimeMgr::onHour([](time_t boundary){
    // boundary is start van het NIEUWE uur (lokale tijd als epoch)
    // RNG::commitHour(boundary);
  });

  TimeMgr::onDay([](time_t boundary){
    // RNG::commitDay(boundary);       // jouw functie
    P1::resetStats();
    // eventueel: meteen Insights pushen
    // JsonFmt::buildInsights(P1Stats); Ws::notifyInsights();
  });
}

void QueueLoop(){
  //todo in workerqueue

  //all events depending on new telegram read
  if ( !P1::newTelegram() ) return;
  P1::clearNewTelegram(); //mark as noticed
  Raw::broadcastLine(P1::RawTelegram + "\n");
  P1::broadcastFields();
  Ws::notifyRawTelegram(P1::RawTelegram);
  Ws::notifyInsights();
  Debug::printf("WS clients: %u\n",Ws::NrClients());
}

void loop() {
  // MQTT::loop();
  Web::loop();
  Storage::loop();
  Led::loop();
  QueueLoop();
  // OTAfw::loop();
  Button::loop();
  NetworkMgr::instance().tick();
  esp_task_wdt_reset();
}