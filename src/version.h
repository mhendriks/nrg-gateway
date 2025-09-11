#pragma once
#define FW_NAME     "NRG Gateway"
// #define FW_VERSION  "6.0.0"

#define _VERSION_MAJOR 4
#define _VERSION_MINOR 15
#define _VERSION_PATCH 11
#define FW_VERSION STR(_VERSION_MAJOR) "." STR(_VERSION_MINOR) "." STR(_VERSION_PATCH)

#define STR1(x) #x
#define STR(x) STR1(x)