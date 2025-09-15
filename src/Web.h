/*

TODO: 
- doorlopen en checken

*/

#pragma once
#include <ESPAsyncWebServer.h>

namespace Web {
void begin();
void loop();
AsyncWebServer& server();
}
