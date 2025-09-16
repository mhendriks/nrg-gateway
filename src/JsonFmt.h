/*

TODO: 
- doorlopen en checken
- uitbreiden andere verzamelingen

*/

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>
#include <FS.h>

namespace JsonFmt {
using Filler = std::function<void(JsonObject)>;

String stringify(Filler fill, size_t reserve = 1024);

// === Builders die je nu gebruikt ===
void buildNow(JsonObject dst);                 // /api/now + WS "now"
void buildInsights(JsonObject dst);
// const String& buildInsights(const stats& S)
void buildRawTelegram(JsonObject dst, const String& text);
void buildSysInfo(JsonObject dst);             // /api/sysinfo
void buildUpdateStatus(JsonObject dst);        // /remote-update response/progress
void buildFileList(JsonObject dst, fs::FS& fs);    // /api/listfiles

// Event wrapper: {"type": "...", "data": {...}}
String wrapEvent(const char* type, Filler fill, size_t reserve = 1024);
}
