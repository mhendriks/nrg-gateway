#pragma once
#include <WiFi.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

#ifdef ETHERNET
  #include <ETH.h>  // jouw huidige ETH init blijft leidend
#endif

#include "Debug.h"      // gebruikt: Debug::println / Debug::printf
#include "Config.h"     // voor defines/profielen
void write2Log(const char* topic, const char* msg, bool alsoSerial); // al aanwezig bij jou

enum class NetworkProfile : uint8_t {
  WiFiOnly,
  EthOnly,
  Ultra  // beide; Ethernet preferred
};

enum class NetState : uint8_t {
  BOOT, TRY_ETH, ETH_WAIT, TRY_WIFI, WIFI_WAIT, ONLINE, START_PORTAL, PORTAL_RUN, BACKOFF
};

struct PortalCfg {
  const char* ssid_prefix = "NRG-Setup-";
  const char* password    = "";   // liever instellen in productie
  IPAddress ap_ip   {192,168,4,1};
  IPAddress ap_gw   {192,168,4,1};
  IPAddress ap_mask {255,255,255,0};
};

struct NetCfg {
  String ssid;
  String pass;
  bool   dhcp = true;
  IPAddress ip, gw, mask, dns1, dns2;
};

class NetworkMgr {
public:
  static NetworkMgr& instance();

  // Aanroepen vanuit main
  void setup(NetworkProfile profile);
  void tick();

  // Status
  bool isOnline() const { return _state == NetState::ONLINE; }
  bool wifiUp()   const { return (WiFi.status() == WL_CONNECTED); }
  bool ethUp()    const;

  // Toegang tot config (bijv. om UI te tonen)
  NetCfg& cfg() { return _cfg; }

private:
  // helpers
  void _loadCfg();
  void _saveCfg();
  void _startWifiSTA();
  void _applyStaDhcpOrStatic();
  void _startPortal();
  void _stopPortal();
  void _installRoutes();
  String _apSsid() const;
  void _maybeBuildScanJson(int count);

private:
  // --- Wi-Fi scan cache (voor /scan) ---
  bool      _scanning = false;
  int       _scanCount = -1;
  uint32_t  _scanT0 = 0;         // wanneer scan gestart / cache opgebouwd
  String    _scanJson;           // JSON cache van laatste scan

  NetworkProfile _profile = NetworkProfile::Ultra;
  NetState  _state   = NetState::BOOT;
  uint32_t  _t0      = 0;
  uint8_t   _retries = 0;

  PortalCfg  _portal;
  NetCfg     _cfg;

  DNSServer      _dns;
  AsyncWebServer _server{80};
  Preferences    _prefs;
  bool           _portalRunning = false;
};
