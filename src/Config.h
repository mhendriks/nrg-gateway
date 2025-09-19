/*

TODO: 
- doorlopen en checken
- detect hardware
- config per hardware / versie opnemen

*/

#pragma once
#include <Arduino.h>

#if !ARDUINO_USB_CDC_ON_BOOT
  extern HWCDC USBSerial;
#else
  #define DEBUG
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
    uint16_t interval = 10;
    String user, pass;
    String base = "nrg";
    bool  tls = false;
  };

  // const char* indexFile = "/index-dev.html"; //debug
  // const char* indexFile = "index.html"; //prod

  struct DevCfg {
#ifdef DEBUG
    String idexfile = "/index-dev.html";
#else 
    String idexfile = "/index.html";
#endif
    uint32_t reboots = 0;
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

  enum class HardwareClass : uint8_t {
  Auto = 0,   // default/unknown -> kies een fallback profiel
  P1U,        // P1 Ultra
  P1UM,       // P1 Ultra Mini
  NRGD,       // NRG Gateway (met Ethernet)
  P1EP,
  P1UX2
  };

  #define FEATURE_LIST(X) \
  X(CORE)                 \
  X(NETSW)                \
  X(VIRT_P1)              \
  X(MODB_BRIDGE)          \
  X(HTTP_POST)

  // ---- Feature types (licht & statisch)
  enum class Feature : uint8_t {
  #define _MAKE_ENUM(name) name,
    FEATURE_LIST(_MAKE_ENUM)
  #undef _MAKE_ENUM
    COUNT
  };

  using FeatureMask = uint64_t;

  struct FeatureSet {
    FeatureMask mask {0};
    constexpr bool has(Feature f) const {
      return mask & (FeatureMask(1ULL) << static_cast<uint8_t>(f));
    }
    void set(Feature f, bool on) {
      FeatureMask bit = (FeatureMask(1ULL) << static_cast<uint8_t>(f));
      if (on) mask |= bit; else mask &= ~bit;
    }
  };

  extern IoCfg  io; 
  extern BtnCfg btn;
  extern NetCfg net;
  extern MqttCfg mqtt;
  extern HwCfg  hw;
  extern DevCfg  dev;

  void setHardwareClass(HardwareClass hw);
  HardwareClass hardwareClass();
  
  const char* hardwareClassStr();
  const char* hardwareClassStr(HardwareClass hw);

  const FeatureSet& features();
  String featuresString(bool enabledOnly = true, const char* sep = ",");
  String featuresJsonObject();
  String featuresJsonArray();

  void load();     // from NVS (stub uses defaults)
  String deviceId();
  String baseTopic();
  const char* getResetReason();

  // helpers
  HardwareSerial& p1Serial();
  inline const char* hostName() { return net.hostname.c_str(); }
}
