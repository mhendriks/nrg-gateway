# NRG Gateway Firmware (v6)

**NRG Gateway firmware (v6)** is the next‑gen software for Smartstuff P1‑dongles.  
It reads DSMR/P1 smart‑meter telegrams and publishes data via Web (UI + WebSockets), REST‑JSON, MQTT, and a raw stream on TCP port 82.  
It also stores local history in ring‑files, supports OTA (web + MQTT), telnet, eFuse‑driven hardware profiles, and optional ESP‑NOW linking to an NRG display.

> Deze README is bedoeld als **developer guide + operator overview**.  
> Gebruik ’m als basis om snel nieuwe builds/varianten te maken die functioneel even rijk zijn als de referentie‑firmware.

---

## Inhoud
- [Belangrijkste features](#belangrijkste-features)
- [Hardware varianten](#hardware-varianten)
- [Architectuuroverzicht](#architectuuroverzicht)
- [Build & flash](#build--flash)
- [Eerste start & setup](#eerste-start--setup)
- [Netwerkdiensten](#netwerkdiensten)
- [REST API](#rest-api)
- [WebSockets](#websockets)
- [MQTT](#mqtt)
- [Ring‑files (lokale historie)](#ring-files-lokale-historie)
- [OTA updates](#ota-updates)
- [Logging, Telnet & FS Explorer](#logging-telnet--fs-explorer)
- [LED & knop gedrag](#led--knop-gedrag)
- [ESP‑NOW (NRG Display)](#esp-now-nrg-display)
- [EnergyID (optioneel)](#energyid-optioneel)
- [Virtual P1 (optioneel)](#virtual-p1-optioneel)
- [Beveiliging](#beveiliging)
- [Troubleshooting](#troubleshooting)
- [Licentie](#licentie)

---

## Belangrijkste features

- **DSMR v2/4/5** uitlezing via de P1‑poort (fork van `dsmr2Lib`).
- **Web UI** in `data/DSMRindexEDGE.html` + **WebSockets** voor realtime updates.
- **REST‑JSON API** + **raw DSMR stream op TCP 82**.
- **MQTT** publish/subscriptions voor integratie (bv. Home Assistant).
- **Lokale opslag** in ring‑files met vaste horizon: **49 uur**, **32 dagen**, **25 maanden** voor:  
  `T1`, `T2`, `T1r` (terug), `T2r` (terug), `gas`, `water`, `accu`, `solar`.
- **OTA** via web en via MQTT (failsafe/rollback).
- **Telnet** console en **FS Explorer** (bestanden beheren).
- **eFuse hardware‑profielen** → automatische IO/LED/ETH/Wi‑Fi mapping (niet meer compile‑time).
- **Knop + LED** (mono of RGB) voor status en acties.
- **Ultra** varianten: **Ethernet + Wi‑Fi**; andere: één van beide.
- **Extra (optioneel):** EnergyID, Virtual P1, Shelly/IO trigger, M‑Bus/Voltage modules, HTTP post van telegrammen.

---

## Hardware varianten

- **ESP32‑C3 – 4 MB Flash** (Wi‑Fi, mono LED)
- **ESP32‑S3 – 8 MB Flash** (Ultra: Wi‑Fi + Ethernet, RGB‑LED)

**eFuse** bevat type/versie → firmware laadt automatisch het juiste **hardwareprofiel** (IO‑mapping, LED‑schema, netwerkopties).
Per profiel kunnen defaults voor web/MQTT/NTP e.d. ingesteld worden (in NVS opslaan).

---

## Architectuuroverzicht

De firmware is modulair en non‑blocking (AsyncWebServer, queues). Hoofdblokken:

1. **Startup**
   - Init **NVS**, reboot‑hook (reden loggen), **ring‑files**, **eFuse → HW‑profiel**.
   - Netwerk init (**Ethernet of Wi‑Fi**, NetSwitch/failover).
   - **NTP** synchronisatie.
   - Print **versie + build‑tijd** op de USB‑console.

2. **P1 lezen (High‑prio)**
   - DSMR parser leest telegrammen (CRC/validatie).
   - Vult **in‑memory statistiek** struct en “last seen” timestamps.

3. **Worker Queue**
   - **MQTT publish** (gecoalesceerd) en **command handling** (via queue).
   - **Ring‑file writes** (batching, wear‑friendly).
   - **ESP‑NOW** push naar NRG‑display.
   - Optioneel: **HTTP Post** van telegrammen (cloud/endpoint).

4. **MQTT proces**
   - Subscriptions → **non‑blocking** command handlers via queue.
   - Publish rate limiting & topic hygiene (retain/QoS waar passend).

5. **Aux proces**
   - **LED** status (mono/RGB) en **knop ISR** (reset, config, bootloader).
   - WDT/failsafe hooks.

---

## Build & flash

**Toolchain**
- **Arduino Core (ESP32) 3.3.0**
- **ArduinoJson** (laatste versie)
- **AsyncTCP** & **ESPAsyncWebServer**
- (Meegebakken) fork van **dsmr2Lib**

**Arduino IDE**
1. Selecteer het juiste board:
   - *ESP32C3 Dev Module* (4MB)
   - *ESP32S3 Dev Module* (8MB) – met juiste ETH pins volgens HW‑profiel
2. Compile & flash de firmware (`.ino` in root).
3. Upload webassets naar SPIFFS/LittleFS (map `data/`).

**Prebuilt binaries**
- Zie **Releases** voor kant‑en‑klare `.bin` files.

---

## Eerste start & setup

1. **Stroom & P1** aansluiten; LED signaleert boot/netwerkstatus.
2. **Netwerk**: Ultra gebruikt ETH wanneer aanwezig, anders Wi‑Fi (of omgekeerd). Andere varianten: alleen Wi‑Fi of alleen ETH.
3. **Web UI**: open `http://<ip>/` → live data en instellingen.
4. **Tijdzone/NTP** en **MQTT** instellen. Credentials worden in **NVS** bewaard.
5. Controleer **WS‑updates** (UI) en **MQTT‑publish** in je broker.

---

## Netwerkdiensten

| Service        | Poort     | Beschrijving                                      |
|----------------|-----------|---------------------------------------------------|
| Web UI (HTTP)  | 80        | AsyncWebServer; `data/DSMRindexEDGE.html`         |
| WebSockets     | 80 (ws)   | Realtime updates naar de UI                       |
| **Raw DSMR**   | **82**    | Tekststream van ruwe telegrammen                  |
| REST API       | 80        | JSON endpoints (zie hieronder)                    |
| MQTT           | 1883/8883 | Publish/subscriptions                             |
| Telnet         | 23        | Console/debug                                     |
| OTA (web)      | 80        | Upload & flash via UI                        |
| OTA (MQTT)     | —         | Trigger via control topic (worker download/flash) |

> **NetSwitch**: op Ultra‑modellen kan de firmware failoveren tussen ETH & Wi‑Fi.

---

## REST API

Basis: `http://<device-ip>/api/v1`

- `GET /now` → actuele snapshot (vermogen, T1/T2, teruglevering, gas, water, accu, solar, timestamps, status).
- `GET /status` → systeemstatus (uptime, rssi, heap, versie, netwerk).
- `GET /history?scope=hour|day|month&key=T1|T2|T1r|T2r|gas|water|accu|solar` → ring‑data (zie ring‑files).
- `POST /cmd` → commando JSON (bv. `{ "op": "reboot" }`, `{ "op": "ota", "url": "http://..." }`).

**Voorbeeld** `GET /now` (ingekort):
```json
{
  "ts": 1725972000,
  "power_w": 1234,
  "t1_kwh": 1234.567,
  "t2_kwh": 890.123,
  "t1r_kwh": 45.678,
  "t2r_kwh": 12.345,
  "gas_m3": 456.789,
  "water_l": 12345,
  "accu": {"soc": 62, "state": "charge"},
  "solar_w": 2100,
  "net": {"link":"eth","rssi":-","ip":"192.168.1.10"},
  "fw": {"name":"NRG Gateway","ver":"6.x"}
}
```

---

## WebSockets

- Kanaal: `ws://<ip>/ws` (door UI gebruikt).
- Berichten: compacte JSON snapshots (zoals `/now`) met hogere frequentie.
- UI verwacht stabiele, backward‑compatible sleutels.

---

## MQTT

**Aanbevolen topics (voorbeeld):**
- `nrg/<device>/state` – actuele snapshot JSON (niet te vaak; throttle).
- `nrg/<device>/tele` – periodieke telemetrie (interval instelbaar).
- `nrg/<device>/availability` – `online`/`offline` (retain).
- `nrg/<device>/cmd/...` – commando’s, bijv. `reboot`, `ota`, `io/set`, `cfg/set`.

**Best practices**
- Combineer velden in één JSON i.p.v. veel losse topics.
- Gebruik `retain` spaarzaam (alleen status/config die zinvol is te cachen).
- QoS 0 of 1 afhankelijk van broker/latency‑eisen.

---

## Ring‑files (lokale historie)

**Horizonnen:**
- **Uur**: 49 records
- **Dag**: 32 records
- **Maand**: 25 records

**Per record**
- Sleutels: `ts`, `T1`, `T2`, `T1r`, `T2r`, `gas`, `water`, `accu`, `solar`.
- Schrijven via **Worker Queue** (batching/merge) om flash wear te beperken.
- **Self‑healing** bij boot: niet‑aflopende datum → detectie & herstel (kopie/interpolatie).

**Filesysteem**
- LittleFS/SPIFFS; bestanden hebben vaste lengte (slots).

---

## OTA updates

### Via Web
- Open UI → **Update** → upload `.bin`.
- Firmware valideert image en flash’t; LED toont progress‑state.

### Via MQTT
- Publiceer commando op control‑topic, bijv.:
  ```json
  { "op":"ota", "url":"http://ota.server/path/firmware.bin" }
  ```
- Worker downloadt stream, verifieert en flash’t.

**Failsafe**
- CRC/offset checks, retries, rollback bij mislukking.
- WDT wordt gereset in progress callbacks.
- Status terug via MQTT/Telnet log.

---

## Logging, Telnet & FS Explorer

- **Telnet (23)**: live logs, eenvoudige commando’s (status, reboot, …).
- **Logbestand**: circulair; bij “vol” → rename naar `.old` en doorgaan met schoon bestand.
- **FS Explorer (web)**: bekijk/download/upload bestanden (config, logs, ring‑files).

---

## LED & knop gedrag

**LED**  
- *Mono*: blink‑codes voor Wi‑Fi connect, ETH link, fout, OTA.  
- *RGB (Ultra)*: kleur per state, bijv. groen=OK, blauw=Wi‑Fi connect, geel=OTA, rood=fout.

**Knop**  
- *Short press*: status/actie (configurable).
- *Long press*: soft‑reboot of Wi‑Fi AP‑mode.
- *Very long*: config reset / bootloader (download mode).

Knop is **ISR‑debounced**; acties verlopen via de queue (niet‑blocking).

---

## ESP‑NOW (NRG Display)

- Koppel NRG‑display via pairing of vast MAC.
- Payload: compacte snapshot (vermogen, fases, planner/prijs, etc.).
- ACK/NACK + retries; werkt naast Wi‑Fi/ETH.

---

## EnergyID (optioneel)

- Koppeling naar een EnergyID‑account.
- Stel token/endpoint in; posts lopen via de Worker Queue.

---

## Virtual P1 (optioneel)

- Simuleer P1‑uitvoer (lab/test).
- Schakelbaar via config/define; gebruikt dezelfde JSON/flows als live data.

---

## Beveiliging

- Stel **MQTT user/pass** en **broker‑ACL’s** in.
- Overweeg **TLS** voor MQTT/HTTP indien resources het toelaten.
- Beperk open services in productienetwerken (bijv. raw port 82 alleen waar nodig).
- **OTA** alleen vanaf vertrouwde endpoints.

---

## Troubleshooting

- **Geen data?** Controleer P1‑kabel/level en meter (DSMR versie).
- **Wi‑Fi issues?** Test met vaste kanaalbreedte, 2.4 GHz; gebruik ETH op Ultra voor stabiliteit.
- **MQTT storm?** Verhoog publish‑interval; bundel velden (één JSON).
- **Ring‑files corrupt?** Herstelroutine draait bij boot; check logs via Telnet/FS Explorer.
- **OTA faalt?** Controleer URL/reachability; kijk naar status op Telnet/MQTT.

---

## Licentie

MIT (zie `LICENSE`).

---

### Credits
- Smartstuff / NRG Gateway firmware team
- Gebaseerd op ESP32 Arduino, AsyncWebServer, ArduinoJson, en een fork van `dsmr2Lib`.



---

## MQTT topicnamen (standaard schema)

**Base-topic**: `nrg/<device_id>`  

Waar **`<device_id>`** standaard de **chip ID** of een door jou ingestelde alias is (alleen [A–Z0–9_-]).

### Outgoing (publish door device)

| Doel | Topic | Retain | Payload |
|---|---|---:|---|
| **Availability (LWT)** | `nrg/<id>/availability` | ✅ | `"online"` bij (re)connect, `"offline"` via LWT bij disconnect |
| **Snapshot (actueel)** | `nrg/<id>/state` | ❌ | JSON, compacte actuele waarden (zie voorbeeld) |
| **Telemetrie (periodiek)** | `nrg/<id>/tele` | ❌ | JSON, periodieke sample (frequentie instelbaar) |
| **Statistiek/health** | `nrg/<id>/health` | ❌ | JSON: heap, uptime, rssi, link, fw, … |
| **Events** | `nrg/<id>/event` | ❌ | JSON: `{"type":"ota_start"}` / `{"type":"p1_crc_error"}` etc. |
| **History (optioneel)** | `nrg/<id>/history/<scope>` | ❌ | JSON: `scope = hour|day|month`, ring-slice |

**Voorbeeld payload `nrg/<id>/state`:**
```json
{
  "ts": 1725972000,
  "power_w": 1234,
  "energy": { "t1_kwh": 1234.567, "t2_kwh": 890.123, "t1r_kwh": 45.678, "t2r_kwh": 12.345 },
  "gas_m3": 456.789,
  "water_l": 12345,
  "accu": { "soc": 62, "state": "charge" },
  "solar_w": 2100,
  "phases": { "l1_w": 420, "l2_w": 410, "l3_w": 404 },
  "net": { "link": "eth", "ip": "192.168.1.10", "rssi": null },
  "fw": { "name": "NRG Gateway", "ver": "6.x" }
}
```

### Incoming (subscribe door device)

| Doel | Topic | Retain | Payload (voorbeeld) |
|---|---|---:|---|
| **OTA trigger** | `nrg/<id>/cmd/ota` | ❌ | `{ "url": "http://ota.server/fw/nrg-v6.bin" }` |
| **Reboot** | `nrg/<id>/cmd/reboot` | ❌ | `{}` of `{"delay":3}` |
| **Config set** | `nrg/<id>/cmd/cfg/set` | ❌ | `{"mqtt":{"host":"...","user":"..."}, "ntp":{"tz":"Europe/Amsterdam"}}` |
| **IO/Shelly** | `nrg/<id>/cmd/io/set` | ❌ | `{"relay":1, "state":"on"}` |
| **Loglevel** | `nrg/<id>/cmd/loglevel` | ❌ | `{"module":"mqtt","level":"debug"}` |
| **Pair ESP-NOW** | `nrg/<id>/cmd/pair` | ❌ | `{"mac":"B0:81:84:3C:B9:10"}` |
| **Post telegram** | `nrg/<id>/cmd/post` | ❌ | `{"url":"https://endpoint/ingest"}` |

**Aanbevelingen**
- **QoS** 0 of 1. Gebruik **retain** alleen voor `availability` (= LWT + “online”).  
- Bundel velden in één JSON per publish om “topic storm” te voorkomen.  
- Base-topic configureerbaar maken (`nrg` → `smartstuff` of tenant-prefix).  
- Voeg indien nodig **discovery** toe voor Home Assistant (`homeassistant/sensor/...`), retained configuratie.

---

## Quickstart – Arduino IDE (inclusief bestandsopdeling)

### 1) Vereisten
- **Arduino IDE 2.x**
- **Boards Manager:** *esp32 by Espressif Systems* **v3.3.0** (exact)  
- **Libraries:**
  - **ESPAsyncWebServer** (compatibel met esp32 v3.3.0)
  - **AsyncTCP**
  - **ArduinoJson** (laatste)
  - (Meegecompileerd) fork van **dsmr2Lib**  
  - Optioneel: **PubSubClient** of andere MQTT lib; **ESPmDNS**; **Update.h** (OTA)

> Tip: pin library versies in `library.properties`/`lib_deps` of documenteer ze in de repo om breuken te voorkomen.

### 2) Board selecteren
- **ESP32-C3 Dev Module** (Flash 4MB) – Wi‑Fi, mono LED  
- **ESP32-S3 Dev Module** (Flash 8MB) – Ultra (Wi‑Fi + Ethernet, RGB LED)

### 3) Partities & FS
- Gebruik de standaard partitie-indeling die **OTA** toelaat (bijv. *Default 4MB with OTA* voor C3).  
- Filesystem: **LittleFS** of **SPIFFS** (consistent met je project).  
- Upload de web UI (`/data`) met de **ESP32 Data Upload** plug-in.

### 4) Project-opdeling (aanbevolen)
Plaats in de sketch map (zelfde niveau als `.ino`):
```
/src
  main.ino
  Config.h
  Debug.h
  version.h

  P1.h            P1.cpp           // DSMR lezen, parsing, MyData in-memory
  Network.h       Network.cpp      // Wi-Fi/Ethernet init, NetSwitch, NTP
  Web.h           Web.cpp          // HTTP routes, REST, WS, raw:82
  MQTT.h          MQTT.cpp         // topics, publish/subscribe, queue-koppeling
  Storage.h       Storage.cpp      // ring-files (hour/day/month), log rollover
  OTAfw.h         OTAfw.cpp        // web-OTA, MQTT-OTA, failsafe/rollback
  StatusLed.h     StatusLed.cpp    // mono/RGB LED states
  Button.h        Button.cpp       // ISR debounced, actions → queue
  EspNow.h        EspNow.cpp       // NRG display koppeling
  PostTlg.h       PostTlg.cpp      // optioneel: HTTP post van telegram
  EnergyID.h      EnergyID.cpp     // optioneel: integratie
```

### 5) Skeletten (korte voorbeelden)

**`main.ino`**
```cpp
#include "Config.h"
#include "Network.h"
#include "P1.h"
#include "Web.h"
#include "MQTT.h"
#include "Storage.h"
#include "StatusLed.h"
#include "Button.h"
#include "OTAfw.h"
#include "EspNow.h"

void setup() {
  Debug.begin(115200);
  StatusLed::begin();
  Storage::begin();           // mount FS, ringfiles check/self-heal
  Config::load();             // NVS → runtime cfg
  Networks::begin();           // ETH/WiFi + NTP
  P1::begin();                // DSMR parser
  Web::begin();               // HTTP + WS + raw:82
  MQTT::begin();              // broker connect, LWT 'offline' → 'online'
  Button::begin();            // ISR, actions → queue
  EspNow::maybeBegin();       // optioneel
}

void loop() {
  P1::loopHighPrio();         // lees/parse DSMR (laat blocking code hier weg)
  MQTT::loop();               // pomp client (indien lib nodig heeft)
  Web::loop();                // events afhandelen (meestal async)
  Storage::loop();            // batch writes ringfiles
  OTAfw::loop();              // eventuele OTA-progress
  StatusLed::loop();          // animaties per state
  Button::loop();             // edge-detectie/long-press timers
}
```

**`MQTT.h`**
```cpp
namespace MQTT {
  void begin();
  void loop();
  void publishState();   // nrg/<id>/state
  void publishTele();    // nrg/<id>/tele
  void publishHealth();  // nrg/<id>/health
  void handleCmd(const char* subtopic, const char* payload, size_t len);
}
```

**`Storage.h`**
```cpp
namespace Storage {
  void begin();
  void loop();
  void writeHour();    // ring 49
  void writeDay();     // ring 32
  void writeMonth();   // ring 25
  void log(const char* tag, const char* msg); // rollover .old bij vol
}
```

**`Web.h`**
```cpp
namespace Web {
  void begin();  // REST /api/v1/*  | WS /ws | RAW :82
  void loop();
}
```

> Richtlijn: **alle netwerk I/O non‑blocking** (Async lib), **alle zware writes via queue**. Houd **P1::loopHighPrio()** licht en voorkom JSON allocs in de P1 ISR/parse callback.

### 6) MQTT in code – vaste topics
```cpp
static String baseTopic() { return String("nrg/") + Config::deviceId(); }

static String t_avail()  { return baseTopic() + "/availability"; }
static String t_state()  { return baseTopic() + "/state"; }
static String t_tele()   { return baseTopic() + "/tele"; }
static String t_health() { return baseTopic() + "/health"; }

static String t_cmd(const char* leaf) {
  return baseTopic() + "/cmd/" + leaf;
}
```

### 7) Build tips
- Schakel **LTO** in waar mogelijk; zet **-Os** voor kleinere binaries.
- Minimaliseer heap-fragmentatie: hergebruik **StaticJsonDocument** waar mogelijk.
- Gebruik **ESP.getChipId()** of eFuse-velden voor `device_id` default.
- Houd **watchdog** in de gaten tijdens OTA en grote FS I/O.
