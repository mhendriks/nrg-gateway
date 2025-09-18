#include "Web.h"
#include "Ws.h"
#include "JsonFmt.h"
#include "Raw.h"
#include <LittleFS.h>
#include "Network.h"
#include <Update.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include "version.h"

namespace { AsyncWebServer g_srv(80); bool g_started=false; }
namespace Web {

AsyncWebServer& server(){ return g_srv; }

// static void handleNow(AsyncWebServerRequest* req){
//   req->send(200,"application/json", JsonFmt::stringify(JsonFmt::buildNow));
// }

static void handleInsights(AsyncWebServerRequest* req){
  req->send(200,"application/json", JsonFmt::stringify(JsonFmt::buildInsights));
}

static void handleSysInfo(AsyncWebServerRequest* req){
  req->send(200,"application/json", JsonFmt::stringify(JsonFmt::buildSysInfo));
}

static void handleListFiles(AsyncWebServerRequest* req){
  String body = JsonFmt::stringify([&](JsonObject o){ JsonFmt::buildFileList(o, LittleFS); }, 4096);
  req->send(200,"application/json", body);
}

static void handleDelete(AsyncWebServerRequest* req){
  if (!req->hasParam("path")) { req->send(400,"text/plain","missing path"); return; }
  String p = req->getParam("path")->value();
  bool ok = LittleFS.remove(p);
  req->send(ok?200:404,"text/plain", ok?"OK":"Not found");
}

static void handleDownload(AsyncWebServerRequest* req){
  if (!req->hasParam("path")) { req->send(400,"text/plain","missing path"); return; }
  String p = req->getParam("path")->value();
  if (!LittleFS.exists(p)) { req->send(404,"text/plain","Not found"); return; }
  req->send(LittleFS, p, String(), true); // force download
}

// (optioneel) OTA/remote-update hook die jouw bestaande flow aanroept
static void handleRemoteUpdate(AsyncWebServerRequest* req){
  // roep je bestaande update routine aan en rapporteer status
  req->send(200,"application/json", JsonFmt::stringify(JsonFmt::buildUpdateStatus));
}

#define HOST_DATA_FILES     "cdn.jsdelivr.net"
#define PATH_DATA_FILES     "https://cdn.jsdelivr.net/gh/mhendriks/nrg-gateway@" STR(_VERSION_MAJOR) "." STR(_VERSION_MINOR) "/data"
#define URL_INDEX_FALLBACK  "https://cdn.jsdelivr.net/gh/mhendriks/nrg-gateway@latest/data"

void GetFile(String filename, String path ){
  
  WiFiClient wifiClient;
  HTTPClient http;

  if(wifiClient.connect(HOST_DATA_FILES, 443)) {
    Debug::println(String(path + filename).c_str());
      http.begin(path + filename);
      int httpResponseCode = http.GET();
//      Serial.print(F("HTTP Response code: "));Serial.println(httpResponseCode);
      if (httpResponseCode == 200 ){
        String payload = http.getString();
  //      Serial.println(payload);
        File file = LittleFS.open(filename, "w"); // open for reading and writing
        if (!file) Debug::println(F("open file FAILED!!!\r\n"));
        else file.print(payload); 
        file.close();
      }
      http.end(); 
      wifiClient.stop(); //end client connection to server  
  } else {
    Debug::println(F("connection to server failed"));
  }
}

void checkFileExist(const char* file){
  if (!LittleFS.exists(file) ) {
    Debug::println(F("Oeps! Index file not pressent, try to download it!"));
    GetFile(file, PATH_DATA_FILES); //download file from cdn
    if (!LittleFS.exists(file) ) {
      Debug::println(F("Oeps! Index file not pressent, try to download it!\r"));
      GetFile(file, URL_INDEX_FALLBACK);
    }
    if (!LittleFS.exists(file) ) { //check again
      Debug::println(F("Index file still not pressent!\r"));
      }
  }
}

void begin() {
  if (g_started) return; g_started = true;
  
  LittleFS.begin();

  const char* indexFile = "/index-dev.html"; //debug
  // const char* indexFile = "index.html"; //prod
  checkFileExist(indexFile);

  g_srv.serveStatic("/", LittleFS, "/").setDefaultFile(indexFile).setCacheControl("max-age=86400");

  // REST
  // g_srv.on("/api/now", HTTP_GET, handleNow);
  g_srv.on("/api/insights", HTTP_GET, handleInsights);
  g_srv.on("/api/sysinfo", HTTP_GET, handleSysInfo);
  g_srv.on("/api/listfiles", HTTP_GET, handleListFiles);
  g_srv.on("/api/fs/delete", HTTP_POST, handleDelete);
  g_srv.on("/api/fs/download", HTTP_GET, handleDownload);
  g_srv.on("/remote-update", HTTP_GET, handleRemoteUpdate);

  // Upload (multipart)
  g_srv.on("/api/fs/upload", HTTP_POST,
    [](AsyncWebServerRequest* req){ req->send(200,"text/plain","OK"); },
    [](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final){
      static File up;
      if (index==0) up = LittleFS.open("/"+filename, "w");
      if (up) up.write(data, len);
      if (final && up) up.close();
    });

  // Start WS + RAW
  Ws::begin(g_srv);
  Raw::begin();

  g_srv.begin();
}

void loop() {
  if (!g_started && NetworkMgr::instance().isOnline()) {
    begin();
  }
}

} // namespace Web
