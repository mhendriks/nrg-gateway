#include "Config.h"
#include <WiFi.h>

#if !ARDUINO_USB_CDC_ON_BOOT
  HWCDC USBSerial;
#else

#endif

namespace Config {
  
  NetCfg net;
  MqttCfg mqtt;
  HwCfg  hw;   
  IoCfg   io;      
  BtnCfg  btn;

  void load() {
    // TODO: load from NVS / JSON
    // Voorbeeld: eFuse/hw-type → pins kiezen
    // if (/* S3 Ultra */) { hw.p1_rx_pin=16; hw.p1_uart=1; }
    // if (/* C3 */)      { hw.p1_rx_pin=7;  hw.p1_uart=1; }
    #ifdef CONFIG_IDF_TARGET_ESP32S3
      io.led_is_rgb = true;
      io.btn_pin = 0;
      io.led_pin = 42;  
      net.use_port_82 = true; //test
    #endif
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
}