#include "Network.h"
#include <esp_wifi.h>   // <-- NODIG voor esp_wifi_get_mode


static IPAddress parseIP(const String& s){ IPAddress ip; ip.fromString(s); return ip; }
static String ipToStr(const IPAddress& ip){ return String(ip[0])+"."+ip[1]+"."+ip[2]+"."+ip[3]; }

static const char FORM_HTML[] PROGMEM = R"HTML(
<!doctype html><html lang="nl"><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>NRG Setup</title>
<style>
:root{--accent:#ff7a00;--bg:#f7f7f7;--card:#fff;--fg:#111;--muted:#666;--line:#e9e9e9}
*{box-sizing:border-box}html,body{margin:0;background:var(--bg);color:var(--fg);
font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Helvetica,Arial,sans-serif}
.container{max-width:620px;margin:32px auto;padding:0 16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;box-shadow:0 4px 16px rgba(0,0,0,.05);overflow:hidden}
.brandbar{display:flex;align-items:center;gap:12px;padding:16px 20px;background:#000;color:#fff}
h1{font-size:18px;margin:0}.sub{margin:4px 0 0 0;font-size:13px;color:#bbb}
.body{padding:20px}
label{display:block;font-size:13px;color:#222;margin:12px 0 6px}
input,select,button{width:100%;padding:12px 14px;border:1px solid var(--line);border-radius:12px;background:#fff;color:#111;font-size:14px}
input:focus,select:focus{outline:2px solid rgba(255,122,0,.25);border-color:var(--accent)}
.row{display:grid;grid-template-columns:1fr;gap:12px}
@media(min-width:640px){.row{grid-template-columns:1fr 1fr}}
hr{border:0;border-top:1px solid var(--line);margin:18px 0}
.help{color:var(--muted);font-size:12px}
.btn{background:var(--accent);border:none;font-weight:700;cursor:pointer}
.btn:hover{filter:brightness(0.98)}.btn.secondary{background:#fff;border:1px solid var(--line)}
.chk{display:flex;align-items:center;gap:8px;margin-top:8px;font-size:13px;color:#muted}
.eye{position:relative}
</style>
</head><body>
<div class="container">
  <div class="card">
    <div class="brandbar">
      <div><h1>NRG Setup</h1><div class="sub">Provisioning portal</div></div>
    </div>
    <div class="body">
      <p class="sub" style="margin:0 0 16px 0">Kies een netwerk of vul handmatig in. Standaard gebruiken we DHCP.</p>

      <div style="display:flex;gap:12px;margin-bottom:8px">
        <button type="button" id="btnScan" class="btn" style="flex:0 0 auto;width:auto;padding:10px 14px">Zoek netwerken</button>
        <select id="ssidList" style="flex:1" aria-label="Beschikbare netwerken"></select>
      </div>

      <form method="POST" action="/save" id="cfgForm" novalidate>
        <label>SSID</label>
        <input id="ssidInput" name="ssid" value="%SSID%" autocomplete="username">

        <label>Wachtwoord</label>
        <div class="eye">
          <input id="passInput" name="pass" type="text" value="%PASS%" autocomplete="current-password">
        </div>
        <div class="chk">
          <input id="togglePw" type="checkbox">
          <span>Verberg wachtwoord</span>
        </div>

        <hr>

        <label class="chk" style="margin:0">
          <input id="dhcp" type="checkbox" name="dhcp" %DHCP%>
          <span>DHCP gebruiken</span>
        </label>

        <div id="staticblk" style="margin-top:8px">
          <div class="row">
            <div><label>IP</label><input name="ip" value="%IP%" inputmode="numeric"></div>
            <div><label>Gateway</label><input name="gw" value="%GW%" inputmode="numeric"></div>
            <div><label>Netmask</label><input name="mask" value="%MASK%" inputmode="numeric"></div>
            <div><label>DNS 1</label><input name="dns1" value="%DNS1%" inputmode="numeric"></div>
          </div>
          <label>DNS 2</label><input name="dns2" value="%DNS2%" inputmode="numeric">
          <div class="help">Laat leeg of 0.0.0.0 om over te slaan.</div>
        </div>

        <div style="display:flex; gap:10px; margin-top:18px">
          <button type="submit" class="btn">Opslaan & verbinden</button>
          <button type="button" id="btnFill" class="btn secondary">Gebruik selectie</button>
        </div>
      </form>
    </div>
  </div>
</div>

<script>
const btn = document.getElementById('btnScan');
const sel = document.getElementById('ssidList');
const ssidInput = document.getElementById('ssidInput');
const passInput = document.getElementById('passInput');
const togglePw = document.getElementById('togglePw');
const dhcp = document.getElementById('dhcp');
const staticblk = document.getElementById('staticblk');
const btnFill = document.getElementById('btnFill');

togglePw.checked = false; passInput.type = 'text';
togglePw.addEventListener('change', ()=>{ passInput.type = togglePw.checked ? 'password' : 'text'; });

function toggleStatic(){ staticblk.style.display = dhcp.checked ? 'none' : 'block'; }
dhcp.addEventListener('change', toggleStatic); toggleStatic();

function applySel(){ if (sel.value) ssidInput.value = sel.value; }
sel.addEventListener('change', applySel);
btnFill.addEventListener('click', applySel);

function renderList(arr){
  sel.innerHTML = "";
  if (!Array.isArray(arr) || arr.length === 0) {
    const o=document.createElement('option'); o.textContent="Geen netwerken gevonden"; o.value="";
    sel.appendChild(o); return;
  }
  arr.sort((a,b)=>b.rssi-a.rssi).forEach(n=>{
    const o=document.createElement('option');
    o.textContent = `${n.ssid || "(verborgen)"} (${n.rssi} dBm)`;
    o.value = n.ssid || "";
    sel.appendChild(o);
  });
  if (sel.options.length && !ssidInput.value) ssidInput.value = sel.options[0].value;
}

btn.onclick = async () => {
  try{
    const r = await fetch('/scan',{cache:'no-store'}); const j = await r.json();
    if (Array.isArray(j)) { renderList(j); return; }
    if (j && j.status==='scanning'){
      let tries=0; const t=setInterval(async ()=>{
        tries++;
        try{ const r2=await fetch('/scan',{cache:'no-store'}); const j2=await r2.json();
             if(Array.isArray(j2)){ clearInterval(t); renderList(j2); }
             else if(tries>20){ clearInterval(t); }
        }catch(e){ clearInterval(t); }
      },500);
    }
  }catch(e){ console.log(e); }
};
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

void NetworkMgr::_prepareWifiOnce() {
  if (_wifiPrepared) return;
  _wifiPrepared = true;

  WiFi.persistent(false);
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_MODE_NULL);
  delay(10);

  WiFi.setSleep(false);
#if defined(WIFI_COUNTRY_EU)
  WiFi.setCountry(WIFI_COUNTRY_EU);
#else
  wifi_country_t eu = { "EU", 1, 13, 0, WIFI_COUNTRY_POLICY_AUTO };
  esp_wifi_set_country(&eu);
#endif
}



bool NetworkMgr::ethUp() const {
#ifdef ETHERNET
  return ETH.linkUp() && ETH.localIP() != IPAddress(0,0,0,0);
#else
  return false;
#endif
}

void NetworkMgr::_loadCfg() {
  bool ok = _prefs.begin("netcfg", true);
  if (!ok) {
    _prefs.end();
    ok = _prefs.begin("netcfg", false); // create if missing
    if (!ok) {
      Debug::println(F("[NET] NVS open failed; using defaults"));
      return;
    }
  }
  _cfg.ssid = _prefs.getString("ssid", "");
  _cfg.pass = _prefs.getString("pass", "");
  _cfg.dhcp = _prefs.getBool  ("dhcp", true);
  _cfg.ip   = IPAddress(_prefs.getUInt("ip",   (uint32_t)0));
  _cfg.gw   = IPAddress(_prefs.getUInt("gw",   (uint32_t)0));
  _cfg.mask = IPAddress(_prefs.getUInt("mask", (uint32_t)0));
  _cfg.dns1 = IPAddress(_prefs.getUInt("dns1", (uint32_t)0));
  _cfg.dns2 = IPAddress(_prefs.getUInt("dns2", (uint32_t)0));
  _prefs.end();
}

void NetworkMgr::_saveCfg() {
  if (!_prefs.begin("netcfg", false)) {
    Debug::println(F("[NET] NVS open(RW) failed; cannot save"));
    return;
  }
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
  if (_staBeginIssued) return;
  WiFi.mode(WIFI_STA);
  _applyStaDhcpOrStatic();
  Debug::printf("[NET] WiFi STA connect SSID='%s' DHCP=%d\n", _cfg.ssid.c_str(), _cfg.dhcp);
  WiFi.begin(_cfg.ssid.c_str(), _cfg.pass.c_str());
  _staBeginIssued = true;
  _staBeginMs = millis();
}

String NetworkMgr::_apSsid() const {
  char macs[7]; // 6 hex + 0
  uint32_t mac = (uint32_t)(ESP.getEfuseMac() & 0xFFFFFF);
  snprintf(macs, sizeof(macs), "%06X", mac);
  return String(_portal.ssid_prefix) + String(macs);
}

void NetworkMgr::_installRoutes() {
  _server->reset();

  auto redir = [this](AsyncWebServerRequest* req){ req->redirect("/"); };

  // /scan (async, cache)
// bovenaan Network.cpp heb je al: #include <esp_wifi.h>

_server->on("/scan", HTTP_GET, [this](AsyncWebServerRequest* req){
  static bool busy = false;
  if (busy) { req->send(200, "application/json", "{\"status\":\"scanning\"}"); return; }
  busy = true;

  Debug::println(F("[NET] /scan -> sync passive start (AP+STA)"));

  // Zorg dat we NIET uit AP vallen: schakel desnoods naar AP+STA
  wifi_mode_t prev; esp_wifi_get_mode(&prev);
  if (prev == WIFI_MODE_AP) {
    WiFi.mode(WIFI_AP_STA);
    delay(20);
  }
  // BELANGRIJK: niet terugzetten naar AP aan het einde — laat AP+STA aan.

  // Sync scan, passief, korte dwell per kanaal
  // Overload: scanNetworks(async, show_hidden, passive, max_ms_per_chan, channel, ssid)
  // channel=0 -> alle kanalen; wil je nóg stabieler: kies alleen AP-kanaal (WiFi.channel())
  uint8_t currentCh = WiFi.channel();     // SoftAP-kanaal
  // Kies hier wat jij wilt:
  // 1) Alleen op AP-kanaal scannen (snelst, popup blijft zeker open, maar je ziet alleen AP-kanaal):
  // int count = WiFi.scanNetworks(false, true, true, 80, currentCh, nullptr);
  // 2) Alle kanalen (kan mini-hiccups geven, maar AP blijft meestal staan):
  int count = WiFi.scanNetworks(false, true, true, 80, 0, nullptr);

  // JSON bouwen
  String json = "[";
  if (count > 0) {
    for (int i = 0; i < count; i++) {
      if (i) json += ",";
      String ssid  = WiFi.SSID(i); ssid.replace("\\","\\\\"); ssid.replace("\"","\\\"");
      bool open    = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
      json += "{";
      json += "\"ssid\":\"" + ssid + "\"";
      json += ",\"bssid\":\"" + WiFi.BSSIDstr(i) + "\"";
      json += ",\"rssi\":" + String(WiFi.RSSI(i));
      json += ",\"ch\":"   + String(WiFi.channel(i));
      json += ",\"open\":" + String(open ? "true" : "false");
      json += "}";
    }
  }
  json += "]";

  Debug::printf("[NET] /scan -> sync passive done: %d nets (AP+STA stays)\n", count);
  // Stuur expliciet Connection: close, zodat captive assistant de sessie netjes afrondt
  AsyncWebServerResponse* res = req->beginResponse(200, "application/json", json);
  res->addHeader("Connection", "close");
  req->send(res);

  busy = false;
});

  // captive portal probes
  _server->on("/generate_204", HTTP_ANY, redir);
  _server->on("/gen_204", HTTP_ANY, redir);
  _server->on("/hotspot-detect.html", HTTP_ANY, redir);
  _server->on("/kindle-wifi/wifistub.html", HTTP_ANY, redir);
  _server->on("/library/test/success.html", HTTP_ANY, redir);
  _server->on("/success.txt", HTTP_ANY, redir);
  _server->on("/ncsi.txt", HTTP_ANY, redir);
  _server->on("/connecttest.txt", HTTP_ANY, redir);
  _server->on("/canonical.html", HTTP_ANY, redir);

  // portal UI
  _server->on("/", HTTP_GET, [this](AsyncWebServerRequest* req){
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

  _server->on("/save", HTTP_POST, [this](AsyncWebServerRequest* req){
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
    _authLatch = false;                 // <--- ontgrendel auto-close
_staAuthFailed = false;
_staBeginIssued = false;
_staFailCount = 0;
_pendingTrySTA = true;              // <--- vraag directe STA retry aan
    req->send(200, "text/html",
      "<meta http-equiv='refresh' content='2;url=/'/>"
      "<body style='font-family:system-ui'>Opgeslagen. Verbinden...</body>");
    _t0 = millis();
  });

  _server->onNotFound([](AsyncWebServerRequest *request){ request->redirect("/"); });
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

  WiFi.onEvent([this](WiFiEvent_t ev, WiFiEventInfo_t info){
  if (ev == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
    auto r = info.wifi_sta_disconnected.reason;
    if (r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT || r == WIFI_REASON_AUTH_EXPIRE) {
      _staAuthFailed = true;
      _authLatch = true;              // blokkeer auto-close
    }
    _staBeginIssued = false;
  } else if (ev == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    _staAuthFailed = false;
    _authLatch = false;               // <--- bij succes weer vrijgeven
    _staFailCount = 0;
  }
});

  _t0 = millis();
  Debug::printf("[NET] setup profile=%d\n", (int)_profile);
}

void NetworkMgr::tick() {
if (_state == NetState::PORTAL_RUN && _dns) _dns->processNextRequest();

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
        // write2Log("NET","online_eth",true);
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
        _prepareWifiOnce();
        _startWifiSTA(); _t0 = millis(); _state = NetState::WIFI_WAIT;
      } else {
        Debug::println(F("[NET] No creds -> START_PORTAL"));
        _state = NetState::START_PORTAL;
      }
    } break;

    case NetState::WIFI_WAIT: {
  if (wifiUp()) {
    // LOG_NET("online_wifi");
    Debug::printf("[NET] WIFI ONLINE ip=%s rssi=%d\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    _state = NetState::ONLINE; _retries = 0; _staFailCount = 0; _staAuthFailed = false; _staBeginIssued=false;
    break;
  }

  // Auth fout? → direct naar portal (of backoff bij EthOnly)
  if (_staAuthFailed) {
    Debug::println(F("[NET] WIFI auth failed → portal"));
    _staAuthFailed = false;
    _staFailCount++;
    WiFi.disconnect(true, true);
    _state = (_profile == NetworkProfile::EthOnly) ? NetState::BACKOFF : NetState::START_PORTAL;
    _t0 = millis();
    break;
  }

  // Time-out op connect poging (bijv. 15s)
  if (millis() - _staBeginMs > 15000) {
    Debug::println(F("[NET] WIFI connect timeout → portal"));
    _staFailCount++;
    WiFi.disconnect(true, true);
    _staBeginIssued = false;
    _state = (_profile == NetworkProfile::EthOnly) ? NetState::BACKOFF : NetState::START_PORTAL;
    _t0 = millis();
    break;
  }
} break;

    case NetState::ONLINE: {
      if (!ethUp() && !wifiUp()) {
        // write2Log("NET","offline",true);
        Debug::println(F("[NET] OFFLINE"));
        _state = NetState::BACKOFF; _t0 = millis();
      }
    } break;

    case NetState::START_PORTAL: {
      if (_profile == NetworkProfile::EthOnly) { _state = NetState::BACKOFF; break; }
      _prepareWifiOnce();
      _startPortal(); _t0 = millis(); _state = NetState::PORTAL_RUN;
    } break;

case NetState::PORTAL_RUN: {
  // Direct na /save naar TRY_WIFI (geen 5s wachten)
  if (_pendingTrySTA) {
    _pendingTrySTA = false;
    _stopPortal();
    _state = NetState::TRY_WIFI;
    break;
  }

  // Oude auto-close, maar alleen als géén auth-latch actief is
  if (_cfg.ssid.length() >= 1 && !_authLatch && (millis() - _t0 > 5000)) {
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

void NetworkMgr::_startPortal() {
  _prepareWifiOnce();
  WiFi.mode(WIFI_AP);  // << AP-only is rustiger
  WiFi.softAPConfig(_portal.ap_ip, _portal.ap_gw, _portal.ap_mask);

  if (!_server) _server = new AsyncWebServer(80);
  if (!_dns)    _dns    = new DNSServer();

  const String apSsid = _apSsid();
  Debug::printf("[NET] Start portal SSID='%s'\n", apSsid.c_str());
  WiFi.softAP(apSsid.c_str(), (strlen(_portal.password)?_portal.password:NULL));

  // wacht op AP-IP
  IPAddress apip;
  for (int i=0;i<50;i++){ apip = WiFi.softAPIP(); if (apip[0] != 0) break; delay(10); }
  delay(150); // mini-settle

  if (apip[0] != 0) _dns->start(53, "*", _portal.ap_ip);

  _installRoutes();
  _server->begin();
  _portalRunning = true;
}

void NetworkMgr::_stopPortal() {
  if (!_portalRunning) return;
  if (_dns)    { _dns->stop();  delete _dns;    _dns = nullptr; }
  if (_server) { _server->end(); delete _server; _server = nullptr; }
  WiFi.softAPdisconnect(true);
  _portalRunning = false;
}


const char* NetworkMgr::linkStr() const {
#ifdef ETHERNET
  if (ethUp()) return "eth";
#endif
  if (wifiUp()) return "wifi";
  return "offline";
}

String NetworkMgr::ipStr() const {
#ifdef ETHERNET
  if (ethUp()) return ETH.localIP().toString();
#endif
  if (wifiUp()) return WiFi.localIP().toString();
  return String("0.0.0.0");
}

int NetworkMgr::wifiRSSI() const {
  return wifiUp() ? WiFi.RSSI() : 0;
}