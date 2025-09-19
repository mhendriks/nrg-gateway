/*

TODO: 
- doorlopen en checken

*/

#pragma once
#include <ESPAsyncWebServer.h>

#define HOST_DATA_FILES     "cdn.jsdelivr.net"
#define PATH_DATA_FILES     "https://cdn.jsdelivr.net/gh/mhendriks/nrg-gateway@" STR(_VERSION_MAJOR) "." STR(_VERSION_MINOR) "/data"
#define URL_INDEX_FALLBACK  "https://cdn.jsdelivr.net/gh/mhendriks/nrg-gateway@latest/data"

namespace Web {

  void begin();
  void loop();
  AsyncWebServer& server();

}
