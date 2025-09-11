#pragma once
#include <Arduino.h>

namespace Networks {
  void begin();   // Wi-Fi / Ethernet + NTP
  bool connected();
  String ip();
  String link();  // "wifi" or "eth"
}
