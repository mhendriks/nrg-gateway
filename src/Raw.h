/*

TODO: 
- doorlopen en checken

*/

#pragma once
#include <Arduino.h>

namespace Raw {
void begin();                         // start AsyncServer(82)
void broadcastLine(const String& s);  // push naar alle clients
}
