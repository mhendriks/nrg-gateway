#include "Web.h"
#include "Config.h"
#include "Network.h"
#include "P1.h"
#include "Debug.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

static AsyncWebServer server(81);
static AsyncWebServer rawSrv(82);
static AsyncWebSocket ws("/ws");

static void handleNow(AsyncWebServerRequest *req) {
  StaticJsonDocument<512> doc;
  doc["ts"] = (uint32_t)time(nullptr);
  doc["power_w"] = P1::powerW();
  JsonObject energy = doc.createNestedObject("energy");
  energy["t1_kwh"] = P1::t1kWh();
  energy["t2_kwh"] = P1::t2kWh();
  energy["t1r_kwh"] = P1::t1rKWh();
  energy["t2r_kwh"] = P1::t2rKWh();
  doc["gas_m3"] = P1::gasM3();
  doc["water_l"] = P1::waterL();
  doc["solar_w"] = P1::solarW();
  JsonObject net = doc.createNestedObject("net");
  net["link"] = NetworkMgr::instance().linkStr();
  net["ip"] = NetworkMgr::instance().ipStr();

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

namespace Web {
  void begin() {
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // REST
    server.on("/api/v1/now", HTTP_GET, handleNow);
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
      req->send(200, "text/plain", "NRG Gateway v6 skeleton - UI placeholder");
    });

    // RAW :82
    rawSrv.on("/", HTTP_GET, [](AsyncWebServerRequest *req){
      req->send(200, "text/plain", "RAW DSMR stream placeholder\n");
    });

    server.begin();
    rawSrv.begin();
    Debug::println("[WEB] HTTP 80 + WS + RAW 82 up");
  }

  void loop() {
    // push periodic WS update (lightweight example)
    static uint32_t t0=0;
    if (millis()-t0>1000) {
      t0 = millis();
      StaticJsonDocument<256> doc;
      doc["ts"] = (uint32_t)time(nullptr);
      doc["power_w"] = P1::powerW();
      String out; serializeJson(doc, out);
      ws.textAll(out);
    }
  }
}
