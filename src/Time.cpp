#include "Time.h"
#include <Arduino.h>   // voor localtime_r/mktime op ESP32 ok; delay niet gebruikt

namespace {
  volatile bool g_inited = false;
  volatile bool g_haveTime = false;

  // Volgende geplande grenzen
  time_t g_nextHour = 0;
  time_t g_nextDay  = 0;

  // Eéndelige flags
  volatile bool g_hourFlag = false;
  volatile bool g_dayFlag  = false;

  // Callback arrays (max N registraties; pas aan naar smaak)
  constexpr int MAX_CBS = 6;
  time_cb_t g_hourCbs[MAX_CBS] = {nullptr};
  time_cb_t g_dayCbs [MAX_CBS] = {nullptr};

  inline bool isTimePlausible(time_t t) {
    // alles na 2000-01-01 is ok om NTP “synced genoeg” te noemen
    return t > 946684800; 
  }

  time_t calcNextHourLocal(time_t now) {
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_min = 0; lt.tm_sec = 0;
    // naar volgende uur
    lt.tm_hour += 1;
    return mktime(&lt);
  }

  time_t calcNextMidnightLocal(time_t now) {
    struct tm lt;
    localtime_r(&now, &lt);
    lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0;
    lt.tm_mday += 1;
    return mktime(&lt);
  }

  void ensureSchedule(time_t now) {
    if (g_nextHour == 0)    g_nextHour = calcNextHourLocal(now);
    if (g_nextDay  == 0)    g_nextDay  = calcNextMidnightLocal(now);
  }

  void fireHour(time_t boundary) {
    g_hourFlag = true;
    for (int i=0;i<MAX_CBS;i++) if (g_hourCbs[i]) g_hourCbs[i](boundary);
  }

  void fireDay(time_t boundary) {
    g_dayFlag = true;
    for (int i=0;i<MAX_CBS;i++) if (g_dayCbs[i]) g_dayCbs[i](boundary);
  }
}

namespace TimeMgr {

  void begin() {
    g_inited = true;
    g_haveTime = false;
    g_nextHour = g_nextDay = 0;
    g_hourFlag = g_dayFlag = false;
    // callbacks blijven staan over begin() heen
  }

  void tick() {
    if (!g_inited) return;

    time_t now = time(nullptr);
    if (!isTimePlausible(now)) {
      g_haveTime = false;
      // laat planningen op 0 staan; bij eerste plausibele tijd plannen we nieuw
      return;
    }

    if (!g_haveTime) {
      g_haveTime = true;
      g_nextHour = g_nextDay = 0; // forceer herplanning bij eerste geldige tijd
    }

    ensureSchedule(now);

    // Uurgrens overschreden?
    if (g_nextHour && now >= g_nextHour) {
      time_t boundary = g_nextHour;
      g_nextHour = calcNextHourLocal(now); // plan de volgende
      fireHour(boundary);
    }

    // Daggrens overschreden?
    if (g_nextDay && now >= g_nextDay) {
      time_t boundary = g_nextDay;
      g_nextDay = calcNextMidnightLocal(now);
      fireDay(boundary);
    }
  }

  void onHour(time_cb_t cb) {
    for (int i=0;i<MAX_CBS;i++) if (!g_hourCbs[i]) { g_hourCbs[i] = cb; return; }
  }

  void onDay(time_cb_t cb) {
    for (int i=0;i<MAX_CBS;i++) if (!g_dayCbs[i]) { g_dayCbs[i] = cb; return; }
  }

  bool hasValidTime() { return g_haveTime; }
  time_t nextHourTs() { return g_nextHour; }
  time_t nextMidnightTs() { return g_nextDay; }

  bool consumeHourFlag() { bool v = g_hourFlag; g_hourFlag = false; return v; }
  bool consumeDayFlag () { bool v = g_dayFlag;  g_dayFlag  = false; return v; }
}
