#include "JsonFmt.h"
#include "P1.h"
#include "version.h"
#include <LittleFS.h>

namespace JsonFmt {

String stringify(Filler fill, size_t reserve) {
  DynamicJsonDocument doc(reserve);
  JsonObject root = doc.to<JsonObject>();
  if (fill) fill(root);
  String out; serializeJson(doc, out);
  return out;
}

String wrapEvent(const char* type, Filler fill, size_t reserve) {
  return stringify([&](JsonObject o){
    o["type"] = type;
    o["ver"] = 1;
    o["ts"]   = (uint32_t)time(nullptr);
    // o["seq"]  = (uint32_t)esp_random();      // optional: sequence/id
    JsonObject d = o.createNestedObject("data");
    if (fill) fill(d);
  }, reserve);
}

void buildNow(JsonObject dst) {
  dst["ts"] = (uint32_t)time(nullptr);
  dst["power_w"] = P1::powerW();
  JsonObject e = dst.createNestedObject("energy");
  e["t1_kwh"]  = P1::t1kWh();
  e["t2_kwh"]  = P1::t2kWh();
  e["t1r_kwh"] = P1::t1rKWh();
  e["t2r_kwh"] = P1::t2rKWh();
}

void buildInsights(JsonObject dst) {
  dst["uptime_s"] = millis()/1000;
  dst["heap_free"] = (uint32_t)ESP.getFreeHeap();
  // Voeg hier je extra “insights” velden toe die je nu al serveert
}

void buildRawTelegram(JsonObject dst, const String& text) {
  dst["text"] = text;
}

void buildSysInfo(JsonObject dst) {
  dst["version"] = FW_VERSION;
  dst["sdk"]     = ESP.getSdkVersion();
  dst["chip"]    = (uint32_t)ESP.getEfuseMac();
  dst["free_heap"]= (uint32_t)ESP.getFreeHeap();
}

void buildUpdateStatus(JsonObject dst) {
  // vul met jouw bestaande statusvelden
  dst["state"]   = "idle";
  dst["progress"]= 0;
}

void buildFileList(JsonObject dst, fs::FS& fs) {
  JsonArray arr = dst.createNestedArray("files");
  File root = fs.open("/");
  File f = root.openNextFile();
  while (f) {
    JsonObject o = arr.createNestedObject();
    o["name"] = f.name();
    o["size"] = (uint32_t)f.size();
    f = root.openNextFile();
  }
}

} // namespace
