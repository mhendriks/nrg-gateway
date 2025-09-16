#include "WS.h"
#include "P1.h"
#include "Debug.h"
#include "Config.h"
#include "Time.h"
#include "MyData.h"     // typedef (includes <dsmr2.h> from fork)
#include <dsmr2.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>

// ===== Core selection: run P1 task on the other core than Arduino loop =====
#if defined(CONFIG_FREERTOS_UNICORE)
  #define P1_TASK_CORE 0
#else
  // Arduino framework usually runs loop() on core 1 → pin P1 on core 0
  #ifndef ARDUINO_RUNNING_CORE
    #define ARDUINO_RUNNING_CORE 1
  #endif
  #if (ARDUINO_RUNNING_CORE == 1)
    #define P1_TASK_CORE 0
  #else
    #define P1_TASK_CORE 1
  #endif
#endif

// ===== Task parameters =====
#ifndef P1_TASK_STACK
  #define P1_TASK_STACK (6*1024)  // increase if parsing needs more stack
#endif
#ifndef P1_TASK_PRIO
  #define P1_TASK_PRIO  6         // higher than most idle/net tasks, but not too high
#endif

namespace {
  TaskHandle_t  s_task = nullptr;
  volatile bool s_run  = false;


  // DSMR v2/v5 detection state
  bool     s_pre40           = false;   // v2/v3 family (9600/7E1) if true
  uint32_t s_P1BaudRate      = 115200;  // 9600 or 115200
  bool     s_checksumEnabled = true;    // passed into parser (v5=true, v2=false)
  // uint32_t p1Success         = 0;
  uint32_t p1Error           = 0;
  uint32_t p1Parsed          = 0;

  // timing/heartbeat
  uint32_t _lastMs   = 0;
  uint32_t _interval = 0;
  bool     gotFrame  = false;

  uint8_t      WaterEquipped = 0;
  uint8_t      GasEquipped = 0;
  bool         isV5meter = true;

  // raw buffer (single-buffer strategy)
  static constexpr size_t BUF_MAX = 2048;
  char   buf[BUF_MAX];
  size_t blen = 0;
  bool   NewTelegram = false;

  // raw callback
  P1::RawSink rawSink = nullptr;
  MyData dsmrData = {};

  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

  // offline thresholds
  static constexpr uint32_t OFFLINE_V5_MS = 3500;
  static constexpr uint32_t OFFLINE_V2_MS = 35000;

  // -------- utilities --------
  static inline bool isHex(char c){
    return (c>='0'&&c<='9')||(c>='A'&&c<='F')||(c>='a'&&c<='f');
  }

  // Find end index of a complete telegram in [b, b+n).
  // - For v5 (checksum on): expect '!' + 4 hex + optional CR/LF
  // - For v2 (checksum off): accept '!' + optional CR/LF
  static size_t findFrameEnd(const char* b, size_t n, bool checksumOn){
    // search last '!' from the tail
    ssize_t bang = -1;
    for (ssize_t i=(ssize_t)n-1; i>=0; --i){
      if (b[i]=='!'){ bang = i; break; }
    }
    if (bang < 0) return 0;  // not found yet

    size_t j = (size_t)bang + 1;

    if (checksumOn) {
      // require 4 hex after '!'
      if (j + 4 > n) return 0;  // not enough bytes yet
      if (!(isHex(b[j]) && isHex(b[j+1]) && isHex(b[j+2]) && isHex(b[j+3]))) return 0;
      j += 4;                   // past CRC
    }
    // optional CR/LF
    if (j < n && b[j] == '\r') ++j;
    if (j < n && b[j] == '\n') ++j;

    return (j <= n) ? j : 0;
  }

  // inline void snapshotSet(float pwr, float t1, float t2, float t1r, float t2r, float gas, float water, float solar) {
  //   portENTER_CRITICAL(&mux);
  //   s_power=pwr; s_t1=t1; s_t2=t2; s_t1r=t1r; s_t2r=t2r; s_gas=gas; s_water=water; s_solar=solar;
  //   portEXIT_CRITICAL(&mux);
  // }
  // inline void snapshotGet(float& pwr, float& t1, float& t2, float& t1r, float& t2r, float& gas, float& water, float& solar) {
  //   portENTER_CRITICAL(&mux);
  //   pwr=s_power; t1=s_t1; t2=s_t2; t1r=s_t1r; t2=s_t2r; gas=s_gas; water=s_water; solar=s_solar;
  //   portEXIT_CRITICAL(&mux);
  // }

byte MbusTypeAvailable(byte type){

  // return first type which is available
  if      ( dsmrData.mbus4_device_type == type ) return 4;
  else if ( dsmrData.mbus3_device_type == type ) return 3;
  else if ( dsmrData.mbus2_device_type == type ) return 2;
  else if ( dsmrData.mbus1_device_type == type ) return 1;
  return 0;
}

void ProcessOnce(){
  Debug::println(F("first time check"));
  // if (dsmrData.identification_present) {
  //   //--- this is a hack! The identification can have a backslash in it
  //   //--- that will ruin javascript processing :-(
  //   for(int i=0; i<dsmrData.identification.length(); i++)
  //   {
  //     if (dsmrData.identification[i] == '\\') dsmrData.identification[i] = '=';
  //   }
  //   smID = dsmrData.identification;
  // } // check id 

  // if ( dsmrData.energy_delivered_total_present && !dsmrData.energy_delivered_tariff1_present ) bUseEtotals = true;
  // if ( ! dsmrData.energy_delivered_tariff1_present && !dsmrData.energy_delivered_total_present ) bWarmteLink = true;
  WaterEquipped = MbusTypeAvailable(7);
  GasEquipped = MbusTypeAvailable(3);
  Debug::printf("mbusWater: %d\r\n", WaterEquipped);
  Debug::printf("mbusGas  : %d\r\n", GasEquipped);
  // ResetStats();
  isV5meter = dsmrData.voltage_l1_present || dsmrData.voltage_l2_present || dsmrData.voltage_l3_present;

}

void PreProcess(){
  //copy BE id in general ID
  if (dsmrData.p1_version_be_present) {
    dsmrData.p1_version = dsmrData.p1_version_be;
    dsmrData.p1_version_present     = true;
    // dsmrData.p1_version_be_present  = false;
  }
  if ( !dsmrData.energy_delivered_total_present ) { dsmrData.energy_delivered_total._value = dsmrData.energy_delivered_tariff1.int_val() + dsmrData.energy_delivered_tariff2.int_val(); dsmrData.energy_delivered_total_present = true; }
  if ( !dsmrData.energy_returned_total_present  ) { dsmrData.energy_returned_total._value = dsmrData.energy_returned_tariff1.int_val() + dsmrData.energy_returned_tariff2.int_val(); dsmrData.energy_returned_total_present = true; }
  if ( !dsmrData.energy_delivered_tariff1_present && dsmrData.energy_delivered_total_present ) { dsmrData.energy_delivered_tariff1 = dsmrData.energy_delivered_total; dsmrData.energy_delivered_tariff1_present = true; }
  if ( !dsmrData.energy_delivered_tariff1_present && dsmrData.energy_returned_total_present ) { dsmrData.energy_returned_tariff1 = dsmrData.energy_returned_total;dsmrData.energy_returned_tariff1_present = true; }
  
  // if (!dsmrData.timestamp_present) {
  //       struct tm tm;
  //       if ( getLocalTime(&tm) ) {
  //         // DSTactive = tm.tm_isdst;
  //         char dt[14];
  //         sprintf(dt, "%02d%02d%02d%02d%02d%02d%s\0\0", (tm.tm_year -100 ), tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_isdst?"S":"W");
  //         dsmrData.timestamp         = dt;
  //         dsmrData.timestamp_present = true;
  //       }       
  // } //!timestamp present

  //cal current more accurate; only works with SMR 5 meters because voltage is only available in V5 meters
  if ( dsmrData.voltage_l1_present && dsmrData.voltage_l1 ){
    dsmrData.current_l1._value = (uint32_t)((dsmrData.power_delivered_l1.int_val() + dsmrData.power_returned_l1.int_val())/dsmrData.voltage_l1*1000);
    dsmrData.current_l1_present = true;
  }
  if ( dsmrData.voltage_l2_present && dsmrData.voltage_l2 ){
    dsmrData.current_l2._value = (uint32_t)((dsmrData.power_delivered_l2.int_val() + dsmrData.power_returned_l2.int_val())/dsmrData.voltage_l2*1000);
    dsmrData.current_l2_present = true;
  }
  if ( dsmrData.voltage_l3_present && dsmrData.voltage_l3 ){
    dsmrData.current_l3._value = (uint32_t)((dsmrData.power_delivered_l3.int_val() + dsmrData.power_returned_l3.int_val())/dsmrData.voltage_l3*1000);
    dsmrData.current_l3_present = true;
  }

  // //-- handle mbus delivered values
  // if (mbusWater) {
  //   waterDelivered = MbusDelivered(mbusWater);
  //   waterDeliveredTimestamp = mbusDeliveredTimestamp;
  // }
  // if (mbusGas) {
  //   gasDelivered = MbusDelivered(mbusGas);
  //   gasDeliveredTimestamp = mbusDeliveredTimestamp;
  // }
}

static constexpr float U_OVER_THRESHOLD_V = 253.0f;  // 230V +10%
  unsigned long startTijdL1 = 0;
  unsigned long startTijdL2 = 0;
  unsigned long startTijdL3 = 0;

  bool overspanningActiefL1 = false;
  bool overspanningActiefL2 = false;
  bool overspanningActiefL3 = false;


  void controleerOverspanning(int spanning, uint32_t &overspanningTotaal, unsigned long &startTijd, bool &overspanning) {
    if (spanning >= U_OVER_THRESHOLD_V) {
      if (!overspanning) {
        startTijd = millis();
        overspanning = true;
      }
    } else {
      if (overspanning) {
        overspanningTotaal += (millis() - startTijd) / 1000;
        overspanning = false;
      }
    }
  }

void updateStats(){
  // bool updated = false;
  if (time(nullptr) < 946684800) return; //ntp not ready

  if ( dsmrData.current_l1_present && ( P1::P1Stats.I1piek == 0xFFFFFFFF || dsmrData.current_l1.int_val() > P1::P1Stats.I1piek) ) P1::P1Stats.I1piek = dsmrData.current_l1.int_val();
  if ( dsmrData.current_l2_present && ( P1::P1Stats.I2piek == 0xFFFFFFFF || dsmrData.current_l2.int_val() > P1::P1Stats.I2piek) ) P1::P1Stats.I2piek =  dsmrData.current_l2.int_val(); 
  if ( dsmrData.current_l3_present && ( P1::P1Stats.I3piek == 0xFFFFFFFF || dsmrData.current_l3.int_val() > P1::P1Stats.I3piek) ) P1::P1Stats.I3piek =  dsmrData.current_l3.int_val();
  
  if ( dsmrData.power_delivered_l1_present) {
    if ( dsmrData.power_delivered_l1.int_val() > P1::P1Stats.P1max ) P1::P1Stats.P1max = dsmrData.power_delivered_l1.int_val();
    else if ( (int32_t)(dsmrData.power_delivered_l1.int_val() - dsmrData.power_returned_l1.int_val()) < P1::P1Stats.P1min ) P1::P1Stats.P1min = (int32_t)(dsmrData.power_delivered_l1.int_val() - dsmrData.power_returned_l1.int_val());
  } 

  if ( dsmrData.power_delivered_l2_present ) {
      if ( dsmrData.power_delivered_l2.int_val() > P1::P1Stats.P2max ) P1::P1Stats.P2max = dsmrData.power_delivered_l2.int_val();
      else if ( (int32_t)(dsmrData.power_delivered_l2.int_val() - dsmrData.power_returned_l2.int_val()) < P1::P1Stats.P2min ) P1::P1Stats.P2min = (int32_t)(dsmrData.power_delivered_l2.int_val() - dsmrData.power_returned_l2.int_val());
  } 
  
  if ( dsmrData.power_delivered_l3_present ){
    if ( dsmrData.power_delivered_l3.int_val() > P1::P1Stats.P3max ) P1::P1Stats.P3max = dsmrData.power_delivered_l3.int_val();
    else if ( (int32_t)(dsmrData.power_delivered_l3.int_val() - dsmrData.power_returned_l3.int_val()) < P1::P1Stats.P3min ) P1::P1Stats.P3min = (int32_t)(dsmrData.power_delivered_l3.int_val() - dsmrData.power_returned_l3.int_val());
  } 

  if ( dsmrData.voltage_l1_present ) {
    if ( dsmrData.voltage_l1.int_val() > P1::P1Stats.U1piek ) P1::P1Stats.U1piek = dsmrData.voltage_l1.int_val();
    else if ( dsmrData.voltage_l1.int_val() < P1::P1Stats.U1min ) P1::P1Stats.U1min = dsmrData.voltage_l1.int_val(); 
    controleerOverspanning(dsmrData.voltage_l1, P1::P1Stats.TU1over, startTijdL1, overspanningActiefL1);
  } 

  if ( dsmrData.voltage_l2_present ) {
    if ( dsmrData.voltage_l2.int_val() > P1::P1Stats.U2piek ) P1::P1Stats.U2piek = dsmrData.voltage_l2.int_val(); 
    else if ( dsmrData.voltage_l2.int_val() < P1::P1Stats.U2min ) P1::P1Stats.U2min = dsmrData.voltage_l2.int_val(); 
    controleerOverspanning(dsmrData.voltage_l2, P1::P1Stats.TU2over, startTijdL2, overspanningActiefL2);
  } 

  if ( dsmrData.voltage_l3_present ) {
    if ( dsmrData.voltage_l3.int_val() > P1::P1Stats.U3piek ) P1::P1Stats.U3piek = dsmrData.voltage_l3.int_val();
    else if ( dsmrData.voltage_l3.int_val() < P1::P1Stats.U3min ) P1::P1Stats.U3min = dsmrData.voltage_l3.int_val(); 
    controleerOverspanning(dsmrData.voltage_l3, P1::P1Stats.TU3over, startTijdL3, overspanningActiefL3);
  } 

  
  
  int h = currentLocalHour();
  if ( h >= 0 && h < 5 && (dsmrData.power_delivered.int_val() < P1::P1Stats.Psluip) && dsmrData.power_delivered.int_val() ) P1::P1Stats.Psluip = dsmrData.power_delivered.int_val();
  if ( P1::P1Stats.StartTime == 0 ) P1::P1Stats.StartTime = time(nullptr);
  

}

  // Parse a complete telegram; use a local MyData to avoid stale String state
  static bool parseDSMR(const char* telegram, size_t len) {
    Debug::printf("P1 parsed: %u error: %u len: %u\n", (unsigned)++p1Parsed, (unsigned)p1Error, (unsigned)len);
    
    MyData dsmr_new = {};
    ParseResult<void> res = P1Parser::parse(&dsmr_new, telegram, len, /*unknown_error*/false, /*checksum*/s_checksumEnabled);
    
    if (res.err) {
      p1Error++;
      Debug::println(res.fullError(telegram, telegram + len).c_str());
      return false;
    }
    
    dsmrData = dsmr_new;
    if ( (p1Parsed - p1Error) == 1) ProcessOnce();
    PreProcess();
    updateStats();
    // auto keep = [](bool present, float v, float prev){ return present ? v : prev; };
    // float pwrW = s_power;
    // if (dsmr.power_delivered_present)  pwrW = dsmr.power_delivered.val() * 1000.0f;
    // // If you want net power, subtract returned:
    // // if (dsmr.power_returned_present)   pwrW -= dsmr.power_returned.val() * 1000.0f;

    // float t1  = keep(dsmr.energy_delivered_tariff1_present, dsmr.energy_delivered_tariff1.val(), s_t1);
    // float t2  = keep(dsmr.energy_delivered_tariff2_present, dsmr.energy_delivered_tariff2.val(), s_t2);
    // float t1r = keep(dsmr.energy_returned_tariff1_present,  dsmr.energy_returned_tariff1.val(),  s_t1r);
    // float t2r = keep(dsmr.energy_returned_tariff2_present,  dsmr.energy_returned_tariff2.val(),  s_t2r);

    // float gas = s_gas;
    // if (dsmr.mbus1_delivered_present) gas = dsmr.mbus1_delivered.val();

    // snapshotSet(pwrW, t1, t2, t1r, t2r, gas, s_water, s_solar);

    // Example: timestamp is a String in your fork; use c_str() for printf
    Debug::printf("time    : %s\n", dsmrData.timestamp.c_str());
    // Debug::printf("P1 power: %f\n", pwrW);
    return true;
  }

  // Detect baud via pulses on RX pin (maps to either 9600 or 115200)
  static void detP1Rate(int rxPin) {
  #ifndef SMR5_ONLY
    pinMode(rxPin, INPUT);
    // Wait (max ~120 s) for activity (high→low) on P1
    int timeout = 0;
    while (digitalRead(rxPin) == HIGH && timeout < 240) { Debug::print("."); delay(500); timeout++; }

    long x=0, rate=10000; // smallest observed HIGH pulse width (µs) dominates
    for (int i=0; i<5; i++) {
      Debug::println();
      Debug::print("x");
      x = pulseIn(rxPin, HIGH, 1000000UL); // 1 s guard
      if (!x) { i--; if (++timeout > 240) break; }
      else rate = (x < rate ? x : rate);
    }
    Debug::println();

    uint32_t br;
         if (rate < 12)   br = 115200;
    else if (rate < 150)  { br = 9600;  s_pre40 = true; }
    else                   br = 115200; // fallback

    s_P1BaudRate = br;
  #else
    s_P1BaudRate = 115200;
    s_pre40 = false;
  #endif

    Debug::print("Baudrate: "); Debug::println(String(s_P1BaudRate).c_str());
  }

  // Open serial port with detected settings; also choose checksum policy
  static void setupSMRport() {
    auto& SER = Config::p1Serial();

    SER.end();
    delay(100);

    // Optional DTR fix pin
    if (Config::hw.p1_dtr_pin >= 0) {
      pinMode(Config::hw.p1_dtr_pin, INPUT_PULLDOWN);
    }

    if (s_P1BaudRate == 9600 || s_pre40) {
      SER.begin(9600, SERIAL_7E1, Config::hw.p1_rx_pin, Config::hw.p1_tx_pin);
      s_checksumEnabled = false;
      Debug::usbprintf("9600 baud / 7E1\n");
    } else {
      SER.begin(115200, SERIAL_8N1, Config::hw.p1_rx_pin, Config::hw.p1_tx_pin);
      s_checksumEnabled = true;
      Debug::usbprintf("115200 baud / 8N1\n");
    }
    SER.setRxInvert(true); // RX only
    _lastMs = millis();
    delay(100);
  }

  // The P1 task: read bytes, peel complete frames, parse, and compact the buffer
  static void P1Task(void*) {
    // 1) Detect baud on RX pin (before opening Serial)
    detP1Rate(Config::hw.p1_rx_pin);
    // 2) Open port + select checksum policy
    setupSMRport();

    blen = 0; gotFrame=false; _interval=0;

    const TickType_t idleTick = pdMS_TO_TICKS(2);
    s_run = true;
    esp_task_wdt_add(NULL);
    while (s_run) {
      auto& SER = Config::p1Serial();
      esp_task_wdt_reset();
      if (SER.available() <= 0) { vTaskDelay(idleTick); continue; }

      // Fill buffer as data arrives
      while (SER.available()) {
        int c = SER.read();
        if (c < 0) break;

        // sync on start-of-telegram
        if (blen == 0 && c != '/') continue;

        if (blen < BUF_MAX - 1) {
          buf[blen++] = (char)c;
        } else {
          // Buffer full without a complete frame: try to resync on the last '/'.
          size_t lastSlash = 0;
          for (size_t i=0; i<blen; ++i) if (buf[i]=='/') lastSlash = i;
          if (lastSlash > 0) {
            size_t rest = blen - lastSlash;
            memmove(buf, buf + lastSlash, rest);
            blen = rest;
          } else {
            blen = 0; // drop buffer if no sync char
          }
          continue;
        }

        // Peel all complete frames currently in the buffer
        while (true) {
          size_t end = findFrameEnd(buf, blen, s_checksumEnabled);
          if (!end) break;

          // raw
          if (rawSink) rawSink(buf, end);

          // timing
          uint32_t now = millis();
          if (_lastMs) {
            uint32_t dt = now - _lastMs;
            _interval = (_interval == 0) ? dt : ( (_interval * 3 + dt) / 4 );
          }
          _lastMs = now;

          // parse
          (void)parseDSMR(buf, end);
          P1::RawTelegram = buf; //copy last raw telegram
          NewTelegram = true;
          gotFrame = true;

          // shift remainder forward
          size_t rest = blen - end;
          if (rest) memmove(buf, buf + end, rest);
          blen = rest;
        }
      }
    }
    vTaskDelete(nullptr);
  }

} // namespace (anon)

namespace P1 {

  String RawTelegram;
  JsonDocument s_fieldsDoc;
 
  Stats P1Stats;


  bool isV5() { return (s_P1BaudRate == 115200) && s_checksumEnabled; }

  void begin() {
    if (running()) return;
    BaseType_t ok = xTaskCreatePinnedToCore(P1Task, "P1", P1_TASK_STACK, nullptr, P1_TASK_PRIO, &s_task, P1_TASK_CORE);
    if (ok != pdPASS) {
      Debug::println("[P1] ERROR: task create failed");
      s_task = nullptr;
    }
  }

  void stop() {
    s_run = false;
    // The task will self-delete. We intentionally don't join here to keep API simple.
    // Optionally, add a wait with timeout if you need a synchronous stop.
  }

  void clearNewTelegram(){
    NewTelegram = false;
  }
  bool newTelegram(){
    return NewTelegram;
  }

  bool running(){ return s_task != nullptr; }

  bool hasFix(){ return gotFrame; }

  bool offline(){
    if (!_lastMs) return true;
    const uint32_t thr = isV5() ? OFFLINE_V5_MS : OFFLINE_V2_MS;
    return (millis()-_lastMs) > thr;
  }

  uint32_t lastTelegramMs(){ return _lastMs; }
  uint32_t intervalMs(){ return _interval ? _interval : (isV5()?1000:10000); }

float powerkW() {
  float w = 0.f;
  portENTER_CRITICAL(&mux);
  w = dsmrData.power_delivered.val() - dsmrData.power_returned.val();
  portEXIT_CRITICAL(&mux);
  return w;
}

float powerW() {
  float w = 0.f;
  portENTER_CRITICAL(&mux);
  if (dsmrData.power_delivered_present) w = dsmrData.power_delivered.val() * 1000.0f; // kW -> W
  portEXIT_CRITICAL(&mux);
  return w;
}

float t1kWh() {
  float v = 0.f;
  portENTER_CRITICAL(&mux);
  if (dsmrData.energy_delivered_tariff1_present) v = dsmrData.energy_delivered_tariff1.val();
  portEXIT_CRITICAL(&mux);
  return v;
}

float t2kWh() {
  float v = 0.f;
  portENTER_CRITICAL(&mux);
  if (dsmrData.energy_delivered_tariff2_present) v = dsmrData.energy_delivered_tariff2.val();
  portEXIT_CRITICAL(&mux);
  return v;
}

float t1rKWh() {
  float v = 0.f;
  portENTER_CRITICAL(&mux);
  if (dsmrData.energy_returned_tariff1_present) v = dsmrData.energy_returned_tariff1.val();
  portEXIT_CRITICAL(&mux);
  return v;
}

float t2rKWh() {
  float v = 0.f;
  portENTER_CRITICAL(&mux);
  if (dsmrData.energy_returned_tariff2_present) v = dsmrData.energy_returned_tariff2.val();
  portEXIT_CRITICAL(&mux);
  return v;
}

float gasM3() {
  // Common case: gas is on M-Bus channel 1 (adjust if your setup differs)
  float v = 0.f;
  portENTER_CRITICAL(&mux);
  if (dsmrData.mbus1_delivered_present) v = dsmrData.mbus1_delivered.val();
  portEXIT_CRITICAL(&mux);
  return v;
}

float waterL() {
  // Heuristic: if your water meter is on another M-Bus channel, take the first present.
  // Units depend on the meter (often m³) – convert to liters upstream if needed.
  float v = 0.f;
  portENTER_CRITICAL(&mux);
  if (dsmrData.mbus2_delivered_present)      v = dsmrData.mbus2_delivered.val();
  else if (dsmrData.mbus3_delivered_present) v = dsmrData.mbus3_delivered.val();
  else if (dsmrData.mbus4_delivered_present) v = dsmrData.mbus4_delivered.val();
  // (intentionally skip mbus1 here because it's commonly gas)
  portEXIT_CRITICAL(&mux);
  return v;
}

float solarW() {
  // Exported power typically corresponds to power_returned (kW)
  float w = 0.f;
  portENTER_CRITICAL(&mux);
  if (dsmrData.power_returned_present) w = dsmrData.power_returned.val() * 1000.0f; // kW -> W
  portEXIT_CRITICAL(&mux);
  return w;
}

  void onRaw(RawSink cb){ rawSink = cb; }

  struct BuildJson {
   template<typename Item>
    void apply(Item &i) {
     char Name[25];
     strncpy(Name,String(Item::name).c_str(),sizeof(Name));

      // if (!isInFieldsArray(Name)) return;
        
        if (i.present()) {          
          // s_fieldsDoc[Name]["value"] = value_to_json(i.val());
          s_fieldsDoc[Name] = value_to_json(i.val());
          // if (String(Item::unit()).length() > 0) s_fieldsDoc[Name]["unit"]  = Item::unit();
        }  //else if (!onlyIfPresent) s_fieldsDoc[Name]["value"] = "-";   
    }
    template<typename Item>
    Item& value_to_json(Item& i) {
      return i;
    }

    double value_to_json(TimestampedFixedValue i) {
      return i.int_val()/1000.0;
    }
    
    double value_to_json(FixedValue i) {
      return i.int_val()/1000.0;
    }
  }; //buildJson

  void broadcastFields(){
    if ( !Ws::NrClients() ) return;
    s_fieldsDoc.clear();
    dsmrData.applyEach(BuildJson());
    s_fieldsDoc["power_total"] = powerkW(); //add power total
    Ws::sendWs("fields", s_fieldsDoc);
  }

  void resetStats(){
  P1Stats = {};
  P1Stats.U1min = P1Stats.U2min = P1Stats.U3min = 0xFFFFFFFFu;
  P1Stats.P1min = P1Stats.P2min = P1Stats.P3min = (int32_t)0xFFFFFFFF; // blijft het “meest negatief”
  P1Stats.Psluip = 0xFFFFFFFFu;
}

} //namespace P1
