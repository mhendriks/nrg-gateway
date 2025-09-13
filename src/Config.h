#pragma once
#include <Arduino.h>

#if !ARDUINO_USB_CDC_ON_BOOT
  extern HWCDC USBSerial;   // je had dit al — oké zolang definitie in één .cpp staat
#endif

// Kies één (via build flag of hier hard-coded):
// #define NET_PROFILE_WIFI_ONLY
// #define NET_PROFILE_ETH_ONLY
// #define NET_PROFILE_ULTRA   // ETH preferred + Wi-Fi fallback + SoftAP portal

#ifndef NET_PROFILE_WIFI_ONLY
#ifndef NET_PROFILE_ETH_ONLY
#ifndef NET_PROFILE_ULTRA
  #define NET_PROFILE_ULTRA
#endif
#endif
#endif

// Optioneel: portal wachtwoord leeg = open; liever iets instellen in productie
#define PORTAL_PASSWORD   ""   // bv. "setup1234"

namespace Config {
  struct NetCfg {
    String ssid, pass;
    String hostname = "nrg-gateway";
    bool use_eth = false; // set by eFuse/hw-profile
    bool use_port_82 = false;
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

 struct IoCfg {
    int   btn_pin         = 0;   // -1 = geen knop
    int   led_pin         = 2;   // -1 = geen (mono) LED
    bool  btn_active_low  = true;
    bool  led_active_high = true;
    bool  led_is_rgb      = false; // Ultras = true, alle andere = false
  };

  struct BtnCfg {
    uint32_t debounce_ms        = 40;
    uint32_t short_ms           = 500;
    uint32_t long_ms            = 2000;
    uint32_t very_long_ms       = 6000;
    uint32_t preview_period_ms  = 800;
  };

  extern IoCfg  io; 
  extern BtnCfg btn;
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
