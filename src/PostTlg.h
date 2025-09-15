/*

TODO: 
- implementeren PostTelegram als optie

*/
#pragma once
#include <Arduino.h>

namespace PostTlg {
  void post(const char* url, const char* body);
}
