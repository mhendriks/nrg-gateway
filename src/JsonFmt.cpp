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

void buildInsights(JsonObject dst){
 
  if ( P1::P1Stats.I1piek != 0xFFFFFFFF ) dst["I1piek"] = P1::P1Stats.I1piek;
  if ( P1::P1Stats.I2piek != 0xFFFFFFFF ) dst["I2piek"] = P1::P1Stats.I2piek;
  if ( P1::P1Stats.I3piek != 0xFFFFFFFF ) dst["I3piek"] = P1::P1Stats.I3piek;
  
  if ( P1::P1Stats.P1min != 0xFFFFFFFF ) {
    dst["P1max"]   = P1::P1Stats.P1max;
    dst["P1min"]   = P1::P1Stats.P1min;
  }
  if ( P1::P1Stats.P2min != 0xFFFFFFFF ) {
    dst["P2max"]   = P1::P1Stats.P2max;
    dst["P2min"]   = P1::P1Stats.P2min;
  }

  if ( P1::P1Stats.P3min != 0xFFFFFFFF ) {
    dst["P3max"]   = P1::P1Stats.P3max;
    dst["P3min"]   = P1::P1Stats.P3min;
  }

  if ( P1::P1Stats.U1min != 0xFFFFFFFF ) {
    dst["U1piek"]  = P1::P1Stats.U1piek;
    dst["U1min"]  = P1::P1Stats.U1min;
    dst["TU1over"] = P1::P1Stats.TU1over;
  }

  if ( P1::P1Stats.U2min != 0xFFFFFFFF ) {
    dst["U2piek"]  = P1::P1Stats.U2piek;
    dst["U2min"]  = P1::P1Stats.U2min;
    dst["TU2over"] = P1::P1Stats.TU2over;
  }
  
  if ( P1::P1Stats.U3min != 0xFFFFFFFF ) {
    dst["U3piek"]  = P1::P1Stats.U3piek;
    dst["U3min"]  = P1::P1Stats.U3min;
    dst["TU3over"] = P1::P1Stats.TU3over;
  }
  
  dst["Psluip"]  = P1::P1Stats.Psluip;
  dst["start_time"] = P1::P1Stats.StartTime;

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
