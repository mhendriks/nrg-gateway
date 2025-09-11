#include "Config.h"
#include <WiFi.h>

// #if  defined(CONFIG_IDF_TARGET_ESP32S3) && !ARDUINO_USB_CDC_ON_BOOT
#if  !ARDUINO_USB_CDC_ON_BOOT
  // #include <USB.h>
  HWCDC USBSerial;
#endif

namespace Config {
  NetCfg net;
  MqttCfg mqtt;

  void load() {
    // TODO: load from NVS / JSON file. For skeleton we just keep defaults.
  }

  String deviceId() {
    // Prefer MAC short id
    uint64_t mac = ESP.getEfuseMac();
    char buf[13];
    snprintf(buf, sizeof(buf), "%08X", (uint32_t)(mac & 0xFFFFFFFF));
    return String(buf);
  }

  String baseTopic() {
    return mqtt.base + "/" + deviceId();
  }
}
