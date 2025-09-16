#include "Debug.h"
#include "WS.h"
#include "JsonFmt.h"
#include <map>
#include <set>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static constexpr uint32_t HB_INTERVAL_MS = 15000;  // elke 15s PING
static constexpr uint32_t HB_TIMEOUT_MS  = 3000;   // PONG binnen 3s verwacht
static constexpr uint8_t  HB_MAX_MISSES  = 2;      // na 2 missers: close

namespace {
  AsyncWebSocket ws("/ws");
  std::map<uint32_t, std::set<String>> subs;

  struct HbState {
    uint32_t lastPongMs = 0;
    uint8_t  misses     = 0;
  };
  std::map<uint32_t, HbState> hb;   // per-client heartbeat state

  void sendJson(AsyncWebSocketClient* c, const char* type,
                std::function<void(JsonObject)> fill, size_t reserve) {
    if (!c || !c->canSend()) return;
    c->text(JsonFmt::wrapEvent(type, fill, reserve));
  }

  bool wants(uint32_t cid, const char* topic) {
    auto it = subs.find(cid);
    return it!=subs.end() && it->second.count(topic);
  }

  void onEvent(AsyncWebSocket*, AsyncWebSocketClient* client,
              AwsEventType type, void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) { 
      subs[client->id()] = {};
      hb[client->id()] = HbState{ millis(), 0 };   // init heartbeat
      return; 
    }

    if (type == WS_EVT_DISCONNECT){ 
      subs.erase(client->id()); 
      hb.erase(client->id());
      return; 
    }

    if (type == WS_EVT_PONG) {
      auto it = hb.find(client->id());
      if (it != hb.end()) {
        it->second.lastPongMs = millis();
        it->second.misses = 0;
      }
      return;
    }
    if (type == WS_EVT_DATA) {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (!info->final || info->opcode != WS_TEXT || info->index!=0 || info->len!=len) return;

      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, data, len)) return;

      String cmd = doc["cmd"]|"", topic = doc["topic"]|"";
      const char* mtype = doc["type"] | "";

      // App-niveau ping (compatibel met dal.js): accepteer {"cmd":"ping"} of {"type":"ping"}
      if (cmd == "ping" || strcmp(mtype, "ping") == 0) {
        Debug::println("ping received");
        uint64_t ts_client = doc["ts_client"] | 0ULL;  // mag ontbreken
        sendJson(client, "pong", [&](JsonObject o){
          if (ts_client) o["ts_client"] = ts_client;   // echo voor RTT
          o["uptime_ms"] = (uint32_t)millis();
          o["ts_dev"]    = (uint32_t)time(nullptr);
        }, 128);
        return;
      }

      // Bestaande subscribe/unsubscribe
      if (cmd=="subscribe"  && topic.length()) {
        subs[client->id()].insert(topic);
        Debug::print("subscribe: ");Debug::println(topic.c_str());
      }
      if (cmd=="unsubscribe" && topic.length()) {
        Debug::print("UN-subscribe topic: ");;Debug::print(topic.c_str());
        if ( topic == "all" ) { subs[client->id()] = {}; Debug::println(" -> ALL UN-subscribed");}
        else { subs[client->id()].erase(topic); Debug::printf(" -> only %s UN-subscribed\n", topic.c_str());}
      }
    }
    }

  static void wsHeartbeatTask(void*){
    for (;;) {
      vTaskDelay(pdMS_TO_TICKS(HB_INTERVAL_MS));
      const uint32_t now = millis();

      // Loop over bekende clients
      for (auto it = hb.begin(); it != hb.end(); ) {
        const uint32_t id = it->first;
        HbState &h = it->second;
        AsyncWebSocketClient* c = ws.client(id);

        // Client weg? Ruim op.
        if (!c) { it = hb.erase(it); continue; }

        // Check of vorige PONG te laat is
        if (now - h.lastPongMs > HB_INTERVAL_MS + HB_TIMEOUT_MS) {
          h.misses++;
          if (h.misses >= HB_MAX_MISSES) {
            c->close();                    // forceer disconnect
            it = hb.erase(it);
            continue;
          }
        }

        // Stuur PING (lege payload is ok; sommige versies vereisen pointer)
        // Gebruik een kleine payload om compat te zijn met oudere versies:
        static uint8_t payload[2] = { 'h','b' };
        c->ping(payload, sizeof(payload));

        ++it;
      }

      ws.cleanupClients();  // ruim gesloten clients op
    }
  }

} //end namespace

namespace Ws {

void begin(AsyncWebServer& server) {
  ws.onEvent(onEvent);
  server.addHandler(&ws);

    xTaskCreatePinnedToCore(
      wsHeartbeatTask, "ws_hb", 4096, nullptr,
      1, nullptr, tskNO_AFFINITY  // of ARDUINO_RUNNING_CORE als je dat gebruikt
    );
}

  void broadcast(const char* type, std::function<void(JsonObject)> fill, size_t reserve) {
    ws.textAll(JsonFmt::wrapEvent(type, fill, reserve));
  }

  // void notifyNow() {
  //   for (const auto& kv : subs) {
  //     uint32_t id = kv.first;
  //     if (!wants(id, "now")) continue;
  //     if (auto* c = ws.client(id)) {
  //       sendJson(c, "now", JsonFmt::buildNow, 1024);
  //     }
  //   }
  // }

  void notifyInsights() {
    for (const auto& kv : subs) {
      uint32_t id = kv.first;
      if (!wants(id, "insights")) continue;
      if (auto* c = ws.client(id)) {
        sendJson(c, "insights", JsonFmt::buildInsights, 1024);
      }
    }
  }

  void notifyRawTelegram(const String& raw) {
    for (const auto& kv : subs) {
      uint32_t id = kv.first;
      if (!wants(id, "raw_telegram")) continue;
      if (auto* c = ws.client(id)) {
        sendJson(c, "raw_telegram",
                [&](JsonObject o){ JsonFmt::buildRawTelegram(o, raw); },
                1024);
      }
    }
  }

  void sendWs(const char* type, const JsonDocument& payload) {
    JsonDocument env;
    env["type"] = type;
    env["ver"] = 1;
    env["ts"] = (uint32_t)time(nullptr);;    
    JsonObject data = env.createNestedObject("data");

    // Belangrijk: gebruik de const variant
    JsonVariantConst src = payload.as<JsonVariantConst>();
    if (!src.isNull()) {
      data.set(src);  // kopieert alle key/values van payload in "data"
    }

    String out;
    serializeJson(env, out);
    ws.textAll(out);
  }

  size_t NrClients(){ return ws.count();   }

}
