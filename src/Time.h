/*
TODO: 
= NONE
*/

#pragma once
#include <time.h>
#include "esp_sntp.h"

// --- Settings
#define NTP_SERVER_1 "europe.pool.ntp.org"
#define NTP_SERVER_2 "time.cloudflare.com"
#define NTP_SERVER_3 "time.google.com"
#define TZ_INFO      "CET-1CEST,M3.5.0/2,M10.5.0/3"
#define NTP_SYNC_INTERVAL_MS (3600UL * 1000UL)  // 1 uur

static bool g_timeInit = false;
static bool DSTactive = false;

static void onTimeSync(struct timeval *tv) {
  time_t now; time(&now);
  struct tm tm; localtime_r(&now, &tm);
  DSTactive = tm.tm_isdst;
  Debug::printf("[NTP] Synced: %04d-%02d-%02d %02d:%02d:%02d (%s)\n",
    tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec,
    DSTactive ? "DST" : "STD");
}

inline void TimeInitOnce() {
  if (g_timeInit) return;
  Debug::println(F("[NTP] Init..."));
  sntp_set_time_sync_notification_cb(onTimeSync);
  sntp_set_sync_interval(NTP_SYNC_INTERVAL_MS);
  // Optioneel: smooth mode (werkt geleidelijker bij grote afwijkingen)
  // sntp_set_sync_mode(SNTP_SYNC_MODE_SMOOTH);

  configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
  g_timeInit = true;
}

inline void TimeKickOnNetworkUp() {
  if (!g_timeInit) TimeInitOnce();
  Debug::println(F("[NTP] Network up → restart SNTP"));
  sntp_restart(); // forceer onmiddellijke sync-poging
}

inline void LogCurrentLocalTime() {
  struct tm tm;
  if (getLocalTime(&tm)) {
    Debug::printf("[TIME] %04d-%02d-%02d %02d:%02d:%02d %s\n",
      tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
      tm.tm_hour, tm.tm_min, tm.tm_sec,
      tm.tm_isdst ? "DST" : "STD");
  } else {
    Debug::println(F("[TIME] not available"));
  }
}
