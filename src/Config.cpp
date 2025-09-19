#include "Config.h"
#include <WiFi.h>

#if !ARDUINO_USB_CDC_ON_BOOT
  HWCDC USBSerial;
#else
#endif

const char *resetReasonsIDF[] PROGMEM = {
    [0]  = "0 - Cannot be determined",
    [1]  = "1 - Power-on reset",
    [2]  = "2 - External pin reset",
    [3]  = "3 - Software reset (esp_restart)",
    [4]  = "4 - Exception/Panic",
    [5]  = "5 - Interrupt watchdog",
    [6]  = "6 - Task watchdog",
    [7]  = "7 - Other watchdog",
    [8]  = "8 - Wake from deep sleep",
    [9]  = "9 - Brownout",
    [10] = "10 - SDIO reset",
    [11] = "11 - USB peripheral reset",
    [12] = "12 - JTAG reset",
    [13] = "13 - eFuse error",
    [14] = "14 - Power glitch detected",
    [15] = "15 - CPU lock-up"
};

namespace Config {
  
  NetCfg  net;
  MqttCfg mqtt;
  HwCfg   hw;   
  IoCfg   io;      
  BtnCfg  btn;
  DevCfg  dev;

  const char* getResetReason() {
    static char buf[50];
    esp_reset_reason_t r = esp_reset_reason();
    if (r < 0 || r > 15 || resetReasonsIDF[r] == nullptr) {
        snprintf(buf, sizeof(buf), "Unknown reset reason (%d)", (int)r);
        return buf;
    }
    return resetReasonsIDF[r];
  }

  void load() {
    // TODO: load from NVS

    #ifdef CONFIG_IDF_TARGET_ESP32S3
      io.led_is_rgb = true;
      io.btn_pin = 0;
      io.led_pin = 42;  
      net.use_port_82 = true; //test
    #endif

    dev.reboots++;

    //todo : detimine hardware and load correct profile
    setHardwareClass(Config::HardwareClass::P1UM);
  }

  String deviceId() {
    uint64_t mac = ESP.getEfuseMac();
    char buf[13];
    snprintf(buf, sizeof(buf), "%08X", (uint32_t)(mac & 0xFFFFFFFF));
    return String(buf);
  }

  String baseTopic() {
    return mqtt.base + "/" + deviceId();
  }

  HardwareSerial& p1Serial() {
  #if ARDUINO_USB_CDC_ON_BOOT && (defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3))
    // Native USB actief: forceer naar echte HW UARTs
    switch (hw.p1_uart) {
      case 2: return Serial2;
      case 1: default: return Serial1;   // map 0 óók naar 1 om type clash te vermijden
    }
  #else
    // Geen native USB (bv. ESP32-C3) of CDC off -> Serial is HardwareSerial
    switch (hw.p1_uart) {
      case 0: return Serial;
      case 2: return Serial2;
      case 1: default: return Serial1;
    }
  #endif
  }

   // ===================== Feature toggles (simpel & statisch) =====================

  static HardwareClass g_hw   = HardwareClass::Auto;
  static FeatureSet    g_active;
  static bool          g_init = false;

  static constexpr FeatureMask featBit(Feature f) {
    return (FeatureMask(1ULL) << static_cast<uint8_t>(f));
  }
  #define FEAT(name) featBit(Feature::name)

  // ===== Profielen per hardware-smaak =====
  static constexpr FeatureMask PROFILE_P1UM  = FEAT(CORE) | FEAT(NETSW) | FEAT(VIRT_P1) | FEAT(MODB_BRIDGE) | FEAT(HTTP_POST);
  static constexpr FeatureMask PROFILE_P1U   = FEAT(CORE) | FEAT(NETSW) | FEAT(VIRT_P1) | FEAT(MODB_BRIDGE) | FEAT(HTTP_POST);
  static constexpr FeatureMask PROFILE_NRGD  = FEAT(CORE);
  static constexpr FeatureMask PROFILE_P1EP  = FEAT(CORE) | FEAT(NETSW) | FEAT(VIRT_P1);
  static constexpr FeatureMask PROFILE_P1UX2 = FEAT(CORE) | FEAT(NETSW) | FEAT(VIRT_P1) | FEAT(MODB_BRIDGE) | FEAT(HTTP_POST);
  static constexpr FeatureMask PROFILE_FALLBACK = PROFILE_P1UM;

  static FeatureSet makeProfile(HardwareClass hwc) {
    FeatureSet fs;
    switch (hwc) {
      case HardwareClass::P1U:   fs.mask = PROFILE_P1U;   break;
      case HardwareClass::P1UM:  fs.mask = PROFILE_P1UM;  break;
      case HardwareClass::NRGD:  fs.mask = PROFILE_NRGD;  break;
      case HardwareClass::P1EP:  fs.mask = PROFILE_P1EP;  break;
      case HardwareClass::P1UX2: fs.mask = PROFILE_P1UX2; break;
      case HardwareClass::Auto:
      default:                   fs.mask = PROFILE_FALLBACK; break;
    }
    return fs;
  }

static const char* kHwClassStr[] = {
  "auto", "p1u", "p1um", "nrgd", "p1ep", "p1ux2"
};

const char* hardwareClassStr() {
  uint8_t idx = static_cast<uint8_t>(g_hw);
  if (idx >= (sizeof(kHwClassStr)/sizeof(kHwClassStr[0]))) return "auto";
  return kHwClassStr[idx];
}

const char* hardwareClassStr(HardwareClass hw) {
  uint8_t idx = static_cast<uint8_t>(hw);
  if (idx >= (sizeof(kHwClassStr)/sizeof(kHwClassStr[0]))) return "auto";
  return kHwClassStr[idx];
}

  void setHardwareClass(HardwareClass hwc) { g_hw = hwc; g_init = false; }
  HardwareClass hardwareClass() { return g_hw; }

  static void ensureInit() {
    if (!g_init) { g_active = makeProfile(g_hw); g_init = true; }
  }

  const FeatureSet& features() {
    ensureInit();
    return g_active;
  }

  // ===== Namen & string helpers (lichtgewicht) =====
  #define _MAKE_NAME(name) #name,
  static const char* kFeatureNames[] = { FEATURE_LIST(_MAKE_NAME) };
  #undef _MAKE_NAME

  static inline const char* featureName(Feature f) {
    uint8_t idx = static_cast<uint8_t>(f);
    if (idx >= static_cast<uint8_t>(Feature::COUNT)) return "UNKNOWN";
    return kFeatureNames[idx];
  }

  String featuresJsonArray() {
    ensureInit();
    String out = "[";
    bool first = true;
    for (uint8_t i = 0; i < static_cast<uint8_t>(Feature::COUNT); ++i) {
      Feature f = static_cast<Feature>(i);
      if (!g_active.has(f)) continue;
      if (!first) out += ",";
      out += "\"";
      out += featureName(f);
      out += "\"";
      first = false;
    }
    out += "]";
    return out;
  }

  String featuresJsonObject() {
    ensureInit();
    String out = "{";
    for (uint8_t i = 0; i < static_cast<uint8_t>(Feature::COUNT); ++i) {
      if (i) out += ",";
      out += "\"";
      out += featureName(static_cast<Feature>(i));
      out += "\":";
      out += g_active.has(static_cast<Feature>(i)) ? "true" : "false";
    }
    out += "}";
    return out;
  }

  String featuresString(bool enabledOnly, const char* sep) {
    ensureInit();
    String out; //bool; first = true;
    for (uint8_t i = 0; i < static_cast<uint8_t>(Feature::COUNT); ++i) {
      Feature f = static_cast<Feature>(i);
      bool on = g_active.has(f);
      if (enabledOnly && !on) continue;
      // if (!first) out += sep;
      out += "[";
      out += featureName(f);
      out += "]";
      // first = false;
    }
    return out;
  }

} // namespace Config
  