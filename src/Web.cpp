#include "Web.h"
#include "Network.h"
#include "P1.h"
#include "Debug.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace {
  AsyncWebServer* g_srv = nullptr;
  bool g_started = false;
}

static AsyncServer rawSrv(82);
static std::vector<AsyncClient*> rawClients;

// static NetServer ws_raw(82);
// static AsyncWebSocket ws("/ws");

// handlers zoals je al had...
static void handleNow(AsyncWebServerRequest* req) {
  StaticJsonDocument<512> doc;
  doc["ts"] = (uint32_t)time(nullptr);
  doc["power_w"] = P1::powerW();
  auto energy = doc.createNestedObject("energy");
  energy["t1_kwh"]  = P1::t1kWh();
  energy["t2_kwh"]  = P1::t2kWh();
  energy["t1r_kwh"] = P1::t1rKWh();
  energy["t2r_kwh"] = P1::t2rKWh();
  doc["gas_m3"]   = P1::gasM3();
  doc["water_l"]  = P1::waterL();
  doc["solar_w"]  = P1::solarW();
  auto net = doc.createNestedObject("net");
  net["link"] = NetworkMgr::instance().linkStr();
  net["ip"]   = NetworkMgr::instance().ipStr();
  String out; serializeJson(doc, out);
  req->send(200, "application/json", out);
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Debug::printf("[WS] Client %u connected\n", client->id());
  } else if (type == WS_EVT_DISCONNECT) {
    Debug::printf("[WS] Client %u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    // optional: handle inbound WS messages
  }
}

// namespace Web {
//   void begin() {
//     // ws.onEvent(onWsEvent);
//     // server.addHandler(&ws);

//     // REST
//     server.on("/api/v1/now", HTTP_GET, handleNow);
//     server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
//       req->send(200, "text/plain", "NRG Gateway v6 skeleton - UI placeholder");
//     });

//     // RAW :82
//     // rawSrv.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
//     //   req->send(200, "text/plain", "RAW DSMR stream placeholder\n");
//     // });

//     server.begin();
//     // rawSrv.begin();
//     Debug::println("[WEB] HTTP 80 + WS + RAW 82 up");
//   }

//   void loop() {
//     // push periodic WS update (lightweight example)
//     static uint32_t t0=0;
//     if (millis()-t0>1000) {
//       t0 = millis();
//       StaticJsonDocument<256> doc;
//       doc["ts"] = (uint32_t)time(nullptr);
//       doc["power_w"] = P1::powerW();
//       String out; serializeJson(doc, out);
//       // ws.textAll(out);
//     }
//   }
// }

namespace Web {
  bool isStarted() { return g_started; }

static void removeClient(AsyncClient* c) {
  auto it = std::find(rawClients.begin(), rawClients.end(), c);
  if (it != rawClients.end()) rawClients.erase(it);
  delete c;
}

static void onClientData(void*, AsyncClient* c, void* data, size_t len) {
  // optioneel: echo of commandoparsing
  c->write((const char*)data, len);
}

static void onNewClient(void*, AsyncClient* c) {
  if (!c) return;
  c->setNoDelay(true);
  c->onDisconnect([](void*, AsyncClient* c){ removeClient(c); }, nullptr);
  c->onError([](void*, AsyncClient* c, int8_t){ removeClient(c); }, nullptr);
  c->onData(onClientData, nullptr);
  rawClients.push_back(c);
  c->write("Welcome\r\n", 9);
}

  void rawPrint() {
    if ( !P1::newTelegram() ) return;
    P1::clearNewTelegram();
    Debug::println("raw print");
    for (auto* c : rawClients) {
      if (c && c->connected()) {
        c->write(P1::RawTelegram.c_str(), P1::RawTelegram.length());
        c->write((const char*)"\r\n", 2); //extra line feed
      }
    }
  }

  void begin() {
    if (g_started) return;

    // Start pas als netwerkstack staat (ETH of Wi-Fi)
    if (!NetworkMgr::instance().isOnline()) {
      Debug::println("[WEB] Net not up yet; postpone start");
      return; // in Web::loop nog eens proberen
    }

    g_srv = new AsyncWebServer(80);
    g_srv->on("/api/v1/now", HTTP_GET, handleNow);
    g_srv->on("/", HTTP_GET, [](auto* req){
      req->send(200, "text/plain", "NRG Gateway v6 skeleton - UI placeholder");
    });

    g_srv->begin();
    g_started = true;
    
    //start port 82 
    // ws_raw.begin();
    rawSrv.onClient(onNewClient, nullptr);
    rawSrv.begin();

    Debug::println("[WEB] HTTP 80 + 82 up");
  }

  void loop() {
    // Als netwerk later online komt, alsnog starten
    if (!g_started && NetworkMgr::instance().isOnline()) {
      begin();
    }
    // hier evt. je WS broadcast timer
  }

  // void PrintPort82(){
  //   //print telegram to dongle port 82
  //   if ( Config::net.use_port_82 ) ws_raw.println( P1::RawTelegram ); 
  // }

}
