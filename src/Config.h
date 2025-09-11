#pragma once
#include <Arduino.h>

namespace Config {
  struct NetCfg {
    String ssid, pass;
    String hostname = "nrg-gw";
    bool use_eth = false; // set by eFuse/hw-profile
  };
  struct MqttCfg {
    String host = "192.168.1.10";
    uint16_t port = 1883;
    String user, pass;
    String base = "nrg";
  };

  extern NetCfg net;
  extern MqttCfg mqtt;

  void load();     // from NVS (stub uses defaults)
  String deviceId();
  String baseTopic(); // e.g. nrg/<device>
  inline String hostName(){return net.hostname;};
}
