#include "Network.h"
#include "Config.h"
#include "Debug.h"
#include <WiFi.h>
#include <esp_sntp.h>
#ifdef ETH_CLK_MODE
#include <ETH.h>
#endif

static bool hasEth = false;
static bool isEthUp = false;

namespace Networks {

  static void initTime() {
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");
    // Wait briefly for time sync
    for (int i=0;i<20;i++){ if (time(nullptr) > 1690000000) break; delay(100); }
  }

  void begin() {
    Debug::println("[NET] begin");

    // Hardware profile could set Config::net.use_eth via eFuse (stub false)
    if (Config::net.use_eth) {
#ifdef ETH_CLK_MODE
      Debug::println("[NET] Trying Ethernet...");
      // TODO: init ETH pins per HW profile
      // ETH.begin(...);
      // Wait link
      uint32_t t0 = millis();
      while (millis() - t0 < 5000) {
        // if (ETH.linkUp()) { isEthUp = true; break; }
        delay(100);
      }
      hasEth = true;
      isEthUp = false; // set true if ETH init is implemented
#endif
    }

    if (!isEthUp) {
      Debug::printf("[NET] WiFi connecting to '%s' ...\n", Config::net.ssid.c_str());
      WiFi.mode(WIFI_STA);
      if (Config::net.hostname.length()) WiFi.setHostname(Config::net.hostname.c_str());
      WiFi.begin(Config::net.ssid.c_str(), Config::net.pass.c_str());
      uint32_t t0 = millis();
      while (WiFi.status()!=WL_CONNECTED && (millis()-t0)<15000) { delay(100); }
      if (WiFi.status()==WL_CONNECTED) {
        Debug::printf("[NET] WiFi OK, ip=%s\n", WiFi.localIP().toString().c_str());
      } else {
        Debug::println("[NET] WiFi failed.");
      }
    }

    initTime();
  }

  bool connected() {
#ifdef ETH_CLK_MODE
    if (isEthUp) return true;
#endif
    return WiFi.status()==WL_CONNECTED;
  }

  String ip() {
#ifdef ETH_CLK_MODE
    if (isEthUp) return "eth-ip";
#endif
    if (WiFi.status()==WL_CONNECTED) return WiFi.localIP().toString();
    return "";
  }

  String link() {
#ifdef ETH_CLK_MODE
    if (isEthUp) return "eth";
#endif
    return "wifi";
  }
}
