#include "MQTT.h"
#include "Config.h"
#include "Network.h"
#include "P1.h"
#include "Debug.h"

#include <WiFiClient.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient wifiClient;
static PubSubClient mqtt(wifiClient);

static String t_avail(){ return Config::baseTopic() + "/availability"; }
static String t_state(){ return Config::baseTopic() + "/state"; }
static String t_tele(){  return Config::baseTopic() + "/tele"; }
static String t_health(){return Config::baseTopic() + "/health"; }

static String t_cmd(const char* leaf){ return Config::baseTopic() + "/cmd/" + String(leaf); }

static void onMessage(char* topic, byte* payload, unsigned int len) {
  String t = topic;
  String p; p.reserve(len);
  for (unsigned int i=0;i<len;i++) p += (char)payload[i];

  Debug::printf("[MQTT] RX %s: %s\n", t.c_str(), p.c_str());

  // Examples:
  if (t == t_cmd("reboot")) {
    ESP.restart();
  } else if (t == t_cmd("ota")) {
    // parse URL and trigger OTA in worker (stub)
  }
}

static void ensureConnected() {
  if (mqtt.connected()) return;
  if (!Networks::connected()) return;

  mqtt.setServer(Config::mqtt.host.c_str(), Config::mqtt.port);
  Debug::printf("[MQTT] Connecting to %s:%u ...\n", Config::mqtt.host.c_str(), Config::mqtt.port);
  String clientId = "nrg-" + Config::deviceId();
  if (mqtt.connect(clientId.c_str(), Config::mqtt.user.c_str(), Config::mqtt.pass.c_str(),
                   t_avail().c_str(), 0, true, "offline")) {
    Debug::println("[MQTT] Connected");
    mqtt.publish(t_avail().c_str(), "online", true);
    mqtt.subscribe(t_cmd("+").c_str()); // wildcard won't work in PubSubClient; subscribe each if needed
    mqtt.subscribe(t_cmd("reboot").c_str());
    mqtt.subscribe(t_cmd("ota").c_str());
    mqtt.subscribe(t_cmd("cfg/set").c_str());
    mqtt.subscribe(t_cmd("io/set").c_str());
    mqtt.subscribe(t_cmd("loglevel").c_str());
    mqtt.subscribe(t_cmd("pair").c_str());
    mqtt.subscribe(t_cmd("post").c_str());
  } else {
    Debug::printf("[MQTT] Connect failed, rc=%d\n", mqtt.state());
  }
}

namespace MQTT {
  void begin() {
    mqtt.setCallback(onMessage);
  }

  void loop() {
    ensureConnected();
    if (mqtt.connected()) mqtt.loop();

    static uint32_t tState=0, tTele=0, tHealth=0;
    if (millis()-tState > 5000) { tState=millis(); publishState(); }
    if (millis()-tTele  > 30000){ tTele =millis(); publishTele(); }
    if (millis()-tHealth> 60000){ tHealth=millis(); publishHealth(); }
  }

  void publishState() {
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
    net["link"] = Networks::link();
    net["ip"] = Networks::ip();
    String out; serializeJson(doc, out);
    mqtt.publish(t_state().c_str(), out.c_str(), false);
  }

  void publishTele() {
    StaticJsonDocument<256> doc;
    doc["ts"] = (uint32_t)time(nullptr);
    doc["power_w"] = P1::powerW();
    String out; serializeJson(doc, out);
    mqtt.publish(t_tele().c_str(), out.c_str(), false);
  }

  void publishHealth() {
    StaticJsonDocument<256> doc;
    doc["uptime_s"] = millis()/1000;
    doc["heap"] = ESP.getFreeHeap();
    String out; serializeJson(doc, out);
    mqtt.publish(t_health().c_str(), out.c_str(), false);
  }
}
