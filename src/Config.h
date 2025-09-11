#pragma once
#include <Arduino.h>

#if !ARDUINO_USB_CDC_ON_BOOT
  extern HWCDC USBSerial;   // je had dit al — oké zolang definitie in één .cpp staat
#endif
\
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

 struct HwCfg {
  int   p1_rx_pin = 18;   // kies per board
  int   p1_tx_pin = 21;   // meestal niet nodig
  int   p1_uart   = 1;    // 0=Serial, 1=Serial1, 2=Serial2
  int   p1_dtr_pin = -1;  // optioneel DTR fix pin (INPUT_PULLDOWN); -1 = niet gebruikt
  };


  extern NetCfg net;
  extern MqttCfg mqtt;
  extern HwCfg  hw;           // <<< NIEUW

  void load();     // from NVS (stub uses defaults)
  String deviceId();
  String baseTopic();

  // helpers
  HardwareSerial& p1Serial();
  inline const char* hostName() { return net.hostname.c_str(); }
}
