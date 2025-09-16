/*

TODO: 
- doorlopen en checken

*/

#pragma once
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <functional>

namespace Ws {
void begin(AsyncWebServer& server);

// Event helpers
void broadcast(const char* type, std::function<void(JsonObject)> fill, size_t reserve = 1024);
// void notifyNow();
void notifyInsights();
void notifyRawTelegram(const String& raw);
// void broadcastP1Fields();
void sendWs(const char* type, const JsonDocument& payload);
size_t NrClients();
}
