#include "Ws.h"
#include "JsonFmt.h"
#include <map>
#include <set>

namespace {
  AsyncWebSocket ws("/ws");
  std::map<uint32_t, std::set<String>> subs;

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
    if (type == WS_EVT_CONNECT) { subs[client->id()] = {}; return; }
    if (type == WS_EVT_DISCONNECT){ subs.erase(client->id()); return; }
    if (type == WS_EVT_DATA) {
      AwsFrameInfo* info = (AwsFrameInfo*)arg;
      if (!info->final || info->opcode != WS_TEXT || info->index!=0 || info->len!=len) return;
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, data, len)) return;
      String cmd = doc["cmd"]|"", topic = doc["topic"]|"";
      if (cmd=="subscribe" && topic.length()) subs[client->id()].insert(topic);
      if (cmd=="unsubscribe"&& topic.length()) subs[client->id()].erase(topic);
    }
  }
}

namespace Ws {
void begin(AsyncWebServer& server) {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

  void broadcast(const char* type, std::function<void(JsonObject)> fill, size_t reserve) {
    ws.textAll(JsonFmt::wrapEvent(type, fill, reserve));
  }

  void notifyNow() {
    for (const auto& kv : subs) {
      uint32_t id = kv.first;
      if (!wants(id, "now")) continue;
      if (auto* c = ws.client(id)) {
        sendJson(c, "now", JsonFmt::buildNow, 1024);  // <-- reserve expliciet
      }
    }
  }

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
