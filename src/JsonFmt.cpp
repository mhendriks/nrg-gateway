#include "MQTT.h"
#include "Config.h"
#include "JsonFmt.h"
#include "P1.h"
#include "version.h"
#include <LittleFS.h>
#include "esp_chip_info.h"
#include "WiFi.h"
#include "Network.h"
#include "Time.h"

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

// void buildNow(JsonObject dst) {
//   dst["ts"] = (uint32_t)time(nullptr);
//   dst["power_w"] = P1::powerW();
//   JsonObject e = dst.createNestedObject("energy");
//   e["t1_kwh"]  = P1::t1kWh();
//   e["t2_kwh"]  = P1::t2kWh();
//   e["t1r_kwh"] = P1::t1rKWh();
//   e["t2r_kwh"] = P1::t2rKWh();
// }

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
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);

  dst["device"]["macaddress_"] = NetworkMgr::instance().macStr();
  dst["device"]["freeheap_byte"] = ESP.getFreeHeap();
  dst["device"]["chipid_"] = ESP.getEfuseMac(); //_getChipId();
  dst["device"]["sdkversion_"] = String( ESP.getSdkVersion() );
  dst["device"]["cpufreq_mhz"] = ESP.getCpuFreqMHz();  
  dst["device"]["sketchsize_byte"] = (uint32_t)(ESP.getSketchSize());
  dst["device"]["freesketchspace_byte"]  = (uint32_t)(ESP.getFreeSketchSpace());
  dst["device"]["flashchipsize_byte"] = (uint32_t)(ESP.getFlashChipSize());
  dst["device"]["FSsize_byte"] = (uint32_t)LittleFS.totalBytes();
  dst["device"]["hw_model_"] = Config::hardwareClassStr();

  dst["updates"]["fwversion_"] = FW_VERSION " ( " __DATE__ " " __TIME__ " )";
  
  dst["build"]["compileoptions_"] = Config::featuresString();
  dst["build"]["indexfile_"] = Config::dev.idexfile;
  

  dst["runtime"]["uptime_"] = TimeMgr::uptime();
  dst["runtime"]["reboots_"] = Config::dev.reboots;
  dst["runtime"]["lastreset_"] = Config::getResetReason();
  
  dst["net"]["hostname_"] = Config::hostName();
  dst["net"]["ipaddress_"] = NetworkMgr::instance().ipStr();
#ifndef ETHERNET
  dst["net"]["ssid_"] = WiFi.SSID();
  dst["net"]["wifirssi_"] = WiFi.RSSI();
#endif

  dst["p1"]["telegramcount_"] = P1::ParseCnt;
  dst["p1"]["telegramerrors_"] = P1::ErrorCnt;
  dst["p1"]["v5_meter_"] = P1::isV5();

  //TODO
  // snprintf(cMsg, sizeof(cMsg), "%s:%04d", settingMQTTbroker, settingMQTTbrokerPort);
  dst["mqtt"]["mqttbroker_"] = Config::mqtt.host + ":" + String( Config::mqtt.port );
  dst["mqtt"]["mqttinterval_sec"] = Config::mqtt.interval;
  dst["mqtt"]["mqttbroker_connected_"] = MQTT::Connected();
  
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
