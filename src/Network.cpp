#include "Network.h"

static IPAddress parseIP(const String& s){ IPAddress ip; ip.fromString(s); return ip; }
static String ipToStr(const IPAddress& ip){ return String(ip[0])+"."+ip[1]+"."+ip[2]+"."+ip[3]; }

static const char FORM_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1">
<title>NRG Setup</title><style>body{font-family:system-ui;margin:2rem;max-width:520px}
label{display:block;margin:.6rem 0 .2rem}input,button{width:100%;padding:.6rem}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:.5rem}.row{margin:.6rem 0}
small{color:#666}</style></head><body>
<h2>Wi-Fi configuratie</h2>
<button type="button" id="btnScan">Zoek netwerken</button>
<select id="ssidList" style="width:100%;margin-top:.6rem"></select>
<script>
const btn = document.getElementById('btnScan');
const sel = document.getElementById('ssidList');
const ssidInput = document.querySelector('input[name="ssid"]');

btn.onclick = async () => {
  sel.innerHTML = "";
  let r = await fetch('/scan');
  let j = await r.json();
  if (j.status === 'scanning') {
    let tries = 0;
    let timer = setInterval(async () => {
      tries++;
      let r2 = await fetch('/scan'); let j2 = await r2.json();
      if (Array.isArray(j2)) {
        clearInterval(timer);
        j2.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
          const o=document.createElement('option');
          o.textContent = `${n.ssid} (${n.rssi} dBm)`;
          o.value = n.ssid;
          sel.appendChild(o);
        });
        sel.onchange = ()=>{ ssidInput.value = sel.value; };
      } else if (tries>10) { clearInterval(timer); }
    }, 700);
  }
};
</script>

<form method="POST" action="/save">
<label>SSID</label><input name="ssid" value="%SSID%">
<label>Wachtwoord</label><input name="pass" type="password" value="%PASS%">
<div class="row"><label><input id="dhcp" type="checkbox" name="dhcp" %DHCP%> DHCP gebruiken</label></div>
<div id="staticblk">
  <div class="grid">
    <div><label>IP</label><input name="ip" value="%IP%"></div>
    <div><label>Gateway</label><input name="gw" value="%GW%"></div>
    <div><label>Netmask</label><input name="mask" value="%MASK%"></div>
    <div><label>DNS 1</label><input name="dns1" value="%DNS1%"></div>
  </div>
  <label>DNS 2</label><input name="dns2" value="%DNS2%">
  <small>Laat leeg of 0.0.0.0 om over te slaan.</small>
</div>
<div class="row"><button type="submit">Opslaan & verbinden</button></div>
</form>
<script>
 const dhcp=document.getElementById('dhcp');
 function toggle(){document.getElementById('staticblk').style.display = dhcp.checked?'none':'block';}
 dhcp.addEventListener('change',toggle); toggle();
</script>
</body></html>
)HTML";

void NetworkMgr::_maybeBuildScanJson(int count) {
  if (count < 0) return;
  // Bouw JSON array met velden: ssid, bssid, rssi, ch, open
  String json = "[";
  for (int i = 0; i < count; i++) {
    if (i) json += ",";
    String ssid  = WiFi.SSID(i);
    String bssid = WiFi.BSSIDstr(i);
    int32_t rssi = WiFi.RSSI(i);
    int32_t ch   = WiFi.channel(i);
    bool open    = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);

    // Escape quotes in SSID
    ssid.replace("\\", "\\\\");
    ssid.replace("\"", "\\\"");

    json += "{";
    json += "\"ssid\":\"" + ssid + "\"";
    json += ",\"bssid\":\"" + bssid + "\"";
    json += ",\"rssi\":" + String(rssi);
    json += ",\"ch\":"   + String(ch);
    json += ",\"open\":" + String(open ? "true" : "false");
    json += "}";
  }
  json += "]";
  _scanJson = json;
  _scanT0 = millis();
  _scanCount = count;
}

NetworkMgr& NetworkMgr::instance(){ static NetworkMgr m; return m; }

bool NetworkMgr::ethUp() const {
#ifdef ETHERNET
  return ETH.linkUp() && ETH.localIP() != IPAddress(0,0,0,0);
#else
  return false;
#endif
}

void NetworkMgr::_loadCfg() {
  _prefs.begin("netcfg", true);
  _cfg.ssid = _prefs.getString("ssid", "");
  _cfg.pass = _prefs.getString("pass", "");
  _cfg.dhcp = _prefs.getBool("dhcp", true);
  _cfg.ip   = IPAddress(_prefs.getUInt("ip",   (uint32_t)0));
  _cfg.gw   = IPAddress(_prefs.getUInt("gw",   (uint32_t)0));
  _cfg.mask = IPAddress(_prefs.getUInt("mask", (uint32_t)0));
  _cfg.dns1 = IPAddress(_prefs.getUInt("dns1", (uint32_t)0));
  _cfg.dns2 = IPAddress(_prefs.getUInt("dns2", (uint32_t)0));
  _prefs.end();
}

void NetworkMgr::_saveCfg() {
  _prefs.begin("netcfg", false);
  _prefs.putString("ssid", _cfg.ssid);
  _prefs.putString("pass", _cfg.pass);
  _prefs.putBool  ("dhcp", _cfg.dhcp);
  _prefs.putUInt  ("ip",   (uint32_t)_cfg.ip);
  _prefs.putUInt  ("gw",   (uint32_t)_cfg.gw);
  _prefs.putUInt  ("mask", (uint32_t)_cfg.mask);
  _prefs.putUInt  ("dns1", (uint32_t)_cfg.dns1);
  _prefs.putUInt  ("dns2", (uint32_t)_cfg.dns2);
  _prefs.end();
}

void NetworkMgr::_applyStaDhcpOrStatic() {
  if (!_cfg.dhcp && (uint32_t)_cfg.ip) {
    if (!WiFi.config(_cfg.ip, _cfg.gw, _cfg.mask, _cfg.dns1, _cfg.dns2)) {
      Debug::println(F("[NET] WiFi.config(static) failed, fallback DHCP"));
      WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }
  } else {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  }
}

void NetworkMgr::_startWifiSTA() {
  WiFi.mode(WIFI_STA);
  _applyStaDhcpOrStatic();
  Debug::printf("[NET] WiFi STA connect SSID='%s' DHCP=%d\n", _cfg.ssid.c_str(), _cfg.dhcp);
  WiFi.begin(_cfg.ssid.c_str(), _cfg.pass.c_str());
}

String NetworkMgr::_apSsid() const {
  char macs[7]; // 6 hex + 0
  uint32_t mac = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF);
  snprintf(macs, sizeof(macs), "%06X", mac);
  return String(_portal.ssid_prefix) + String(macs);
}

void NetworkMgr::_installRoutes() {
  _server.reset();

// ---- Captive portal probe redirects ----
  auto redir = [this](AsyncWebServerRequest* req){
    req->redirect("/"); // stuur naar portal-home
  };

  // ---- /scan: niet-blokkerend, met cache ----
  _server.on("/scan", HTTP_GET, [this](AsyncWebServerRequest* req){
    // Cache geldig als < 10s oud en we hebben resultaten
    if (_scanCount >= 0 && (millis() - _scanT0) < 10000) {
      req->send(200, "application/json", _scanJson);
      return;
    }

    // Als al een scan bezig is, antwoord "scanning"
    if (_scanning) {
      req->send(200, "application/json", "{\"status\":\"scanning\"}");
      return;
    }

    // Start een asynchrone scan (true = async). Let op: dit blokkeert niet.
    _scanning = true;
    _scanCount = -1;
    _scanT0 = millis();
    WiFi.scanDelete();          // opruimen oude resultaten
    WiFi.scanNetworks(true);    // start async scan
    req->send(200, "application/json", "{\"status\":\"scanning\"}");
  });

  _server.on("/generate_204", HTTP_ANY, redir);        // Android
  _server.on("/gen_204",      HTTP_ANY, redir);        // Android alt
  _server.on("/hotspot-detect.html", HTTP_ANY, redir); // Apple
  _server.on("/kindle-wifi/wifistub.html", HTTP_ANY, redir);
  _server.on("/library/test/success.html", HTTP_ANY, redir);
  _server.on("/success.txt",  HTTP_ANY, redir);        // Windows alt
  _server.on("/ncsi.txt",     HTTP_ANY, redir);        // Windows NCSI
  _server.on("/connecttest.txt", HTTP_ANY, redir);     // Windows alt
  _server.on("/canonical.html", HTTP_ANY, redir);      // diverse

  _server.on("/", HTTP_GET, [this](AsyncWebServerRequest* req){
    String html = FPSTR(FORM_HTML);
    html.replace("%SSID%", String(_cfg.ssid));
    html.replace("%PASS%", String(_cfg.pass));
    html.replace("%DHCP%", _cfg.dhcp ? "checked" : "");
    html.replace("%IP%",   ipToStr(_cfg.ip));
    html.replace("%GW%",   ipToStr(_cfg.gw));
    html.replace("%MASK%", ipToStr(_cfg.mask));
    html.replace("%DNS1%", ipToStr(_cfg.dns1));
    html.replace("%DNS2%", ipToStr(_cfg.dns2));
    req->send(200, "text/html", html);
  });

  _server.on("/save", HTTP_POST, [this](AsyncWebServerRequest* req){
    auto get = [&](const char* name)->String {
      if (req->hasParam(name, true)) return req->getParam(name, true)->value();
      return String();
    };
    _cfg.ssid = get("ssid");
    _cfg.pass = get("pass");
    _cfg.dhcp = req->hasParam("dhcp", true);
    if (!_cfg.dhcp) {
      _cfg.ip   = parseIP(get("ip"));
      _cfg.gw   = parseIP(get("gw"));
      _cfg.mask = parseIP(get("mask"));
      _cfg.dns1 = parseIP(get("dns1"));
      _cfg.dns2 = parseIP(get("dns2"));
    } else {
      _cfg.ip = _cfg.gw = _cfg.mask = _cfg.dns1 = _cfg.dns2 = IPAddress(0,0,0,0);
    }
    _saveCfg();
    req->send(200, "text/html",
      "<meta http-equiv='refresh' content='2;url=/'/>"
      "<body style='font-family:system-ui'>Opgeslagen. Verbinden...</body>");
    _t0 = millis(); // hint voor retry richting STA in tick()
  });

  _server.onNotFound([](AsyncWebServerRequest *request){
    request->redirect("/");
  });
}

void NetworkMgr::_startPortal() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(_portal.ap_ip, _portal.ap_gw, _portal.ap_mask);
  String apSsid = _apSsid();
  Debug::printf("[NET] Start portal SSID='%s'\n", apSsid.c_str());
  WiFi.softAP(apSsid.c_str(), (strlen(_portal.password)?_portal.password:NULL));

  _dns.start(53, "*", _portal.ap_ip);
  _installRoutes();
  _server.begin();

  _portalRunning = true;
  write2Log("NET","portal_start",true);
}

void NetworkMgr::_stopPortal() {
  if (!_portalRunning) return;
  _dns.stop();
  _server.end();
  WiFi.softAPdisconnect(true);
  _portalRunning = false;
  write2Log("NET","portal_stop",true);
}

void NetworkMgr::setup(NetworkProfile profile) {
  _profile = profile;
  _loadCfg();

#ifdef ETHERNET
  // let op: jouw bestaande ETH init wordt elders gedaan; hier niets blokkerends
#endif

  // Start-state: EthOnly/Ultra -> TRY_ETH, WiFiOnly -> TRY_WIFI
  switch (_profile) {
    case NetworkProfile::EthOnly:  _state = NetState::TRY_ETH;  break;
    case NetworkProfile::WiFiOnly: _state = NetState::TRY_WIFI; break;
    case NetworkProfile::Ultra:    _state = NetState::TRY_ETH;  break;
  }
  _t0 = millis();
  Debug::printf("[NET] setup profile=%d\n", (int)_profile);
}

void NetworkMgr::tick() {
  if (_state == NetState::PORTAL_RUN) _dns.processNextRequest();

    // ---- Afhandeling van asynchrone Wi-Fi scan ----
  if (_scanning) {
    int res = WiFi.scanComplete(); // -1: busy, -2: failed, >=0: aantal netwerken
    if (res >= 0) {
      _maybeBuildScanJson(res);    // bouw cache JSON
      _scanning = false;
      Debug::printf("[NET] scan complete: %d networks\n", res);
    } else if (res == WIFI_SCAN_FAILED) {
      _scanning = false;
      _scanCount = 0;
      _scanJson = "[]";
      _scanT0 = millis();
      Debug::println(F("[NET] scan failed"));
    }
    // res == WIFI_SCAN_RUNNING (-1): niets doen; wachten
  }


  switch (_state) {
    case NetState::TRY_ETH: {
#ifdef ETHERNET
      Debug::println(F("[NET] TRY_ETH"));
      _t0 = millis();
      _state = NetState::ETH_WAIT;
#else
      _state = (_profile == NetworkProfile::EthOnly) ? NetState::BACKOFF : NetState::TRY_WIFI;
#endif
    } break;

    case NetState::ETH_WAIT: {
      if (ethUp()) {
        write2Log("NET","online_eth",true);
        Debug::println(F("[NET] ETH ONLINE"));
        _state = NetState::ONLINE; _retries = 0;
        break;
      }
      if (millis() - _t0 > 10000) { // 10s
        _state = (_profile == NetworkProfile::EthOnly) ? NetState::BACKOFF : NetState::TRY_WIFI;
      }
    } break;

    case NetState::TRY_WIFI: {
      if (_profile == NetworkProfile::EthOnly) { _state = NetState::BACKOFF; break; }
      if (_cfg.ssid.length() >= 1) {
        Debug::println(F("[NET] TRY_WIFI (known creds)"));
        _startWifiSTA(); _t0 = millis(); _state = NetState::WIFI_WAIT;
      } else {
        Debug::println(F("[NET] No creds -> START_PORTAL"));
        _state = NetState::START_PORTAL;
      }
    } break;

    case NetState::WIFI_WAIT: {
      if (wifiUp()) {
        write2Log("NET","online_wifi",true);
        Debug::printf("[NET] WIFI ONLINE ip=%s rssi=%d\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        _state = NetState::ONLINE; _retries = 0;
      } else if (millis() - _t0 > 15000) {
        _state = (_profile == NetworkProfile::EthOnly) ? NetState::BACKOFF : NetState::START_PORTAL;
      }
    } break;

    case NetState::ONLINE: {
      if (!ethUp() && !wifiUp()) {
        write2Log("NET","offline",true);
        Debug::println(F("[NET] OFFLINE"));
        _state = NetState::BACKOFF; _t0 = millis();
      }
    } break;

    case NetState::START_PORTAL: {
      if (_profile == NetworkProfile::EthOnly) { _state = NetState::BACKOFF; break; }
      _startPortal(); _t0 = millis(); _state = NetState::PORTAL_RUN;
    } break;

    case NetState::PORTAL_RUN: {
      if (_cfg.ssid.length() >= 1 && (millis() - _t0 > 5000)) {
        _stopPortal();
        _state = NetState::TRY_WIFI;
        break;
      }
      if (millis() - _t0 > 300000) { // 5 min
        _stopPortal();
        _state = NetState::BACKOFF; _t0 = millis();
      }
    } break;

    case NetState::BACKOFF: {
      if (millis() - _t0 > (1000u * (1 + _retries))) {
        _retries = (_retries < 5) ? _retries + 1 : 5;
        if      (_profile == NetworkProfile::EthOnly)  _state = NetState::TRY_ETH;
        else if (_profile == NetworkProfile::WiFiOnly) _state = NetState::TRY_WIFI;
        else                                           _state = NetState::TRY_ETH; // Ultra: ETH preferred
      }
    } break;

    default: break;
  }
}
