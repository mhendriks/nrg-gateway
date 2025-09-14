#include "Web.h"
#include "Network.h"
#include "P1.h"
#include "Debug.h"
#include "version.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <LittleFS.h>
#include <vector>
#include <algorithm>

namespace {
  AsyncWebServer* g_srv = nullptr;
  bool g_started = false;
}

static AsyncServer rawSrv(82);
static std::vector<AsyncClient*> rawClients;
static AsyncWebSocket ws("/ws");

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

static void ListFiles(AsyncWebServerRequest* req) {
  struct Entry { String name; uint32_t size; };
  std::vector<Entry> list;
  list.reserve(30);

  // Lees directory
  File root = LittleFS.open("/");
  if (!root) {
    JsonDocument err;
    err["error"] = "Failed to open LittleFS root";
    String out; serializeJson(err, out);
    req->send(500, "application/json", out);
    return;
  }

  for (File f = root.openNextFile(); f && list.size() < 30; f = root.openNextFile()) {
    if (f.isDirectory()) continue;
    Entry e{ String(f.name()), static_cast<uint32_t>(f.size()) };
    list.push_back(std::move(e));
  }

  // Sorteer alfabetisch (case-insensitive)
  std::sort(list.begin(), list.end(), [](const Entry& a, const Entry& b) {
    String al = a.name, bl = b.name;
    al.toLowerCase(); bl.toLowerCase();
    return al < bl;
  });

  // FS stats (met 5% “safety headroom” zoals je oude code)
  const uint64_t used = LittleFS.usedBytes();
  const uint64_t total = LittleFS.totalBytes();
  const uint64_t usedHeadroom = (used * 105 + 99) / 100;  // 1.05 afgerond
  const uint64_t freeBytes = (total > usedHeadroom) ? (total - usedHeadroom) : 0;

  // Bouw JSON
  JsonDocument doc;  // ArduinoJson v7 -> dynamisch
  auto files = doc["files"].to<JsonArray>();
  for (const auto& e : list) {
    auto item = files.add<JsonObject>();
    item["name"] = e.name;
    item["size"] = e.size;  // bytes (integer)
  }

  auto fs = doc["fs"].to<JsonObject>();
  fs["usedBytes"] = used;                 // raw
  fs["usedBytesWithHeadroom"] = usedHeadroom; // used * 1.05
  fs["totalBytes"] = total;
  fs["freeBytes"] = freeBytes;

  // Stuur response
  String out;
  serializeJson(doc, out);
  req->send(200, "application/json", out);
}

static void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Debug::printf("[WS] Client %u connected total %u\n", client->id(), server->count() );
  } else if (type == WS_EVT_DISCONNECT) {
    Debug::printf("[WS] Client %u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    // optional: handle inbound WS messages
  }
}

namespace Web {
  bool isStarted() { return g_started; }

// ---- Websocket START

static String jsonPack(const char* type, void (*fill)(JsonObject)) {
  JsonDocument doc;
  // JsonObject root = doc.to<JsonObject>();
  doc["type"] = type;                        // berichttype
  doc["proto"] = 1;
  doc["ts"]   = (uint32_t)time(nullptr);     // optioneel: timestamp
  doc["seq"]  = (uint32_t)esp_random();      // optioneel: sequence/id
  JsonObject data = doc.createNestedObject("data");
  if (fill) fill(data);
  String out; serializeJson(doc, out);
  return out;
}

  static void wsSendTo(AsyncWebSocketClient* c, const char* type, void (*fill)(JsonObject)) {
  if (c && c->status() == WS_CONNECTED) c->text(jsonPack(type, fill));
}

static void wsBroadcast(const char* type, void (*fill)(JsonObject)) {
  ws.textAll(jsonPack(type, fill));
}

// ————— Voorbeelden van zenden
void pushTelemetry() {
  wsBroadcast("telemetry", [](JsonObject d){
    d["power_w"] = P1::powerW();
    JsonObject e = d.createNestedObject("energy");
    e["t1_kwh"] = P1::t1kWh();
    e["t2_kwh"] = P1::t2kWh();
  });
}
// ---- Websocket END

// PORT 82

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
  // c->write("Welcome\r\n", 9);
}

  void rawPrint() {
    if ( !P1::newTelegram() ) return;
    P1::clearNewTelegram();
    // Debug::println("raw print");
    for (auto* c : rawClients) {
      if (c && c->connected()) {
        c->write(P1::RawTelegram.c_str(), P1::RawTelegram.length());
        c->write((const char*)"\r\n", 2); //extra line feed
      }
    }
    wsBroadcast("raw_telegram", [](JsonObject d){ d["text"] = P1::RawTelegram; });
    ws.cleanupClients();
  }

#define HOST_DATA_FILES     "cdn.jsdelivr.net"
#define PATH_DATA_FILES     "https://cdn.jsdelivr.net/gh/mhendriks/nrg-gateway@" STR(_VERSION_MAJOR) "." STR(_VERSION_MINOR) "/data"
#define URL_INDEX_FALLBACK  "https://cdn.jsdelivr.net/gh/mhendriks/nrg-gateway@latest/data"

// #define URL_INDEX_FALLBACK  "http://localhost/~martijn/nrg-gateway/dev" //TEST ONLY

void GetFile(String filename, String path ){
  
  WiFiClient wifiClient;
  HTTPClient http;

  if(wifiClient.connect(HOST_DATA_FILES, 443)) {
    Debug::println(String(path + filename).c_str());
      http.begin(path + filename);
      int httpResponseCode = http.GET();
//      Serial.print(F("HTTP Response code: "));Serial.println(httpResponseCode);
      if (httpResponseCode == 200 ){
        String payload = http.getString();
  //      Serial.println(payload);
        File file = LittleFS.open(filename, "w"); // open for reading and writing
        if (!file) Debug::println(F("open file FAILED!!!\r\n"));
        else file.print(payload); 
        file.close();
      }
      http.end(); 
      wifiClient.stop(); //end client connection to server  
  } else {
    Debug::println(F("connection to server failed"));
  }
}

void checkIndexFile(){
  if (!LittleFS.exists("/index.html") ) {
    Debug::println(F("Oeps! Index file not pressent, try to download it!"));
    GetFile("/index.html", PATH_DATA_FILES); //download file from cdn
    if (!LittleFS.exists("/index.html") ) {
      Debug::println(F("Oeps! Index file not pressent, try to download it!\r"));
      GetFile("/index.html", URL_INDEX_FALLBACK);
    }
    if (!LittleFS.exists("/index.html") ) { //check again
      Debug::println(F("Index file still not pressent!\r"));
      }
  }
 
  // if (!LittleFS.exists("/Frontend.json", false) ) {
  //   DebugTln(F("Frontend.json not pressent, try to download it!"));
  //   GetFile("/Frontend.json", PATH_DATA_FILES);
  // }

  }

  void begin() {
    if (g_started) return;

    // Start pas als netwerkstack staat (ETH of Wi-Fi)
    if (!NetworkMgr::instance().isOnline()) {
      Debug::println("[WEB] Net not up yet; postpone start");
      return; // in Web::loop nog eens proberen
    }
    
    LittleFS.remove("/index.html");
    checkIndexFile();

    g_srv = new AsyncWebServer(80);
    g_srv->on("/api/v1/now", HTTP_GET, handleNow);
    g_srv->on("/api/listfiles", HTTP_GET, ListFiles);
    g_srv->serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    
    // g_srv->on("/", HTTP_GET, [](auto* req){
    //   req->send(200, "text/plain", "NRG Gateway v6 skeleton - UI placeholder");
    // });

    //webserver
    g_srv->begin();
    g_started = true;
    
    //port 82
    rawSrv.onClient(onNewClient, nullptr);
    rawSrv.begin();
    
    //websocket
    ws.onEvent(onWsEvent);
    g_srv->addHandler(&ws);

    Debug::println("[WEB] HTTP 80 + 82 + WS up");
  }

  void loop() {
    if (!g_started && NetworkMgr::instance().isOnline()) begin();
  }

}
