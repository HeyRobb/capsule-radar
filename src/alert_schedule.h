#pragma once
// "Only alert between these times": the schedule for the audio alerts (new-aircraft ping,
// emergency/military beep, proximity beep).
//
// Pure helpers, kept out of main.cpp so tests/alert_schedule_test.cpp can exercise them on
// the host. Every time here is whole minutes since LOCAL midnight (0..1439); main.cpp reads
// the local clock (the web page's time zone, NTP or the RTC) and passes it in.
#include <stddef.h>
#include "config.h"

static const int ALERT_MINUTES_PER_DAY = 24 * 60;

// Parse the value of an <input type=time>: "HH:MM" (also "H:MM", and "HH:MM:SS" with the
// seconds ignored, which some browsers send). Rejects empty input, trailing garbage, an
// hour over 23 and a minute over 59. Returns false (leaving *out untouched) on failure.
inline bool parseClockMinutes(const char *s, int *out) {
    if (!s) return false;
    const char *p = s;
    int hour = 0, hourDigits = 0;
    while (*p >= '0' && *p <= '9' && hourDigits < 2) { hour = hour * 10 + (*p - '0'); ++p; ++hourDigits; }
    if (hourDigits == 0 || *p != ':') return false;
    ++p;
    if (!(p[0] >= '0' && p[0] <= '9' && p[1] >= '0' && p[1] <= '9')) return false;   // exactly 2 digits
    const int minute = (p[0] - '0') * 10 + (p[1] - '0');
    p += 2;
    if (*p == ':') {                                  // optional ":SS", ignored
        if (!(p[1] >= '0' && p[1] <= '9' && p[2] >= '0' && p[2] <= '9')) return false;
        if ((p[1] - '0') * 10 + (p[2] - '0') > 59) return false;
        p += 3;
    }
    if (*p != '\0') return false;                     // anything left over
    if (hour > 23 || minute > 59) return false;
    *out = hour * 60 + minute;
    return true;
}

// A stored value that can't be a time of day (a corrupt NVS int) becomes `fallback`;
// anything else is left as the user set it.
inline int repairClockMinutes(int minutes, int fallback) {
    return (minutes < 0 || minutes >= ALERT_MINUTES_PER_DAY) ? fallback : minutes;
}

// Is `nowMin` inside the window that starts at `startMin` and stops at `stopMin`?
// The start is included and the stop is not: "08:00 to 22:00" alerts through 21:59.
// A window with the stop before the start crosses midnight ("22:00 to 06:00" = the night).
// Start == stop leaves no window to be outside of, so it means all day (no restriction).
inline bool alertWindowContains(int startMin, int stopMin, int nowMin) {
    if (startMin == stopMin) return true;
    if (startMin < stopMin)  return nowMin >= startMin && nowMin < stopMin;
    return nowMin >= startMin || nowMin < stopMin;
}

// May an alert sound right now? Off -> always. On, but the clock isn't set yet (no NTP and
// no RTC time) -> yes as well: a schedule that can't be checked must not silence the alerts
// without anyone knowing. Otherwise only inside the window.
inline bool alertScheduleAllows(bool enabled, int startMin, int stopMin, bool haveTime, int nowMin) {
    if (!enabled || !haveTime) return true;
    return alertWindowContains(startMin, stopMin, nowMin);
}
