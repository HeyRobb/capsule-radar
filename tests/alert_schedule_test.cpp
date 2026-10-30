// Alert schedule ("only alert between these times", src/alert_schedule.h): the HH:MM
// parsing of the web page's time inputs and the window check that gates the audio alerts,
// including windows that cross midnight and a clock that isn't set yet.
#include "alert_schedule.h"

#include <assert.h>

static int hm(int h, int m) { return h * 60 + m; }

int main() {
    int v = -1;

    // --- parseClockMinutes: what <input type=time> sends ---
    assert(parseClockMinutes("08:00", &v) && v == hm(8, 0));
    assert(parseClockMinutes("22:30", &v) && v == hm(22, 30));
    assert(parseClockMinutes("00:00", &v) && v == 0);
    assert(parseClockMinutes("23:59", &v) && v == ALERT_MINUTES_PER_DAY - 1);
    assert(parseClockMinutes("8:05", &v)  && v == hm(8, 5));      // single-digit hour
    assert(parseClockMinutes("07:15:30", &v) && v == hm(7, 15));  // seconds are ignored
    assert(parseClockMinutes("07:15:00", &v) && v == hm(7, 15));

    // --- rejected, leaving the caller's value alone ---
    int keep = 77;
    assert(!parseClockMinutes(nullptr, &keep) && keep == 77);
    assert(!parseClockMinutes("", &keep));
    assert(!parseClockMinutes("24:00", &keep));    // hour out of range
    assert(!parseClockMinutes("12:60", &keep));    // minute out of range
    assert(!parseClockMinutes("99:99", &keep));
    assert(!parseClockMinutes("1200", &keep));     // no separator
    assert(!parseClockMinutes("12:5", &keep));     // one-digit minute
    assert(!parseClockMinutes("123:00", &keep));   // three-digit hour
    assert(!parseClockMinutes(":30", &keep));      // no hour
    assert(!parseClockMinutes("12:", &keep));
    assert(!parseClockMinutes("ab:cd", &keep));
    assert(!parseClockMinutes("12:30pm", &keep));  // trailing garbage
    assert(!parseClockMinutes("12:30:", &keep));   // dangling seconds separator
    assert(!parseClockMinutes("12:30:7", &keep));  // one-digit seconds
    assert(!parseClockMinutes("12:30:61", &keep)); // seconds out of range
    assert(!parseClockMinutes("12:30:00x", &keep));
    assert(!parseClockMinutes(" 12:30", &keep));   // leading space
    assert(!parseClockMinutes("-1:30", &keep));
    assert(keep == 77);

    // --- repairClockMinutes: only values that can't be a time of day change ---
    assert(repairClockMinutes(0, 480) == 0);
    assert(repairClockMinutes(1439, 480) == 1439);
    assert(repairClockMinutes(600, 480) == 600);
    assert(repairClockMinutes(-1, 480) == 480);
    assert(repairClockMinutes(1440, 480) == 480);
    assert(repairClockMinutes(99999, 480) == 480);

    // --- same-day window 08:00-22:00: start included, stop excluded ---
    const int s = hm(8, 0), e = hm(22, 0);
    assert(!alertWindowContains(s, e, hm(7, 59)));
    assert( alertWindowContains(s, e, hm(8, 0)));
    assert( alertWindowContains(s, e, hm(12, 0)));
    assert( alertWindowContains(s, e, hm(21, 59)));
    assert(!alertWindowContains(s, e, hm(22, 0)));
    assert(!alertWindowContains(s, e, hm(23, 59)));
    assert(!alertWindowContains(s, e, 0));              // midnight is outside

    // --- window across midnight 22:00-06:00 (the night) ---
    const int ns = hm(22, 0), ne = hm(6, 0);
    assert( alertWindowContains(ns, ne, hm(22, 0)));
    assert( alertWindowContains(ns, ne, hm(23, 59)));
    assert( alertWindowContains(ns, ne, 0));            // midnight is inside
    assert( alertWindowContains(ns, ne, hm(5, 59)));
    assert(!alertWindowContains(ns, ne, hm(6, 0)));
    assert(!alertWindowContains(ns, ne, hm(12, 0)));
    assert(!alertWindowContains(ns, ne, hm(21, 59)));

    // --- a window that touches midnight on either side ---
    assert( alertWindowContains(0, hm(6, 0), 0));              // 00:00-06:00
    assert( alertWindowContains(0, hm(6, 0), hm(5, 59)));
    assert(!alertWindowContains(0, hm(6, 0), hm(6, 0)));
    assert( alertWindowContains(hm(18, 0), 0, hm(23, 59)));    // 18:00-00:00 = until midnight
    assert(!alertWindowContains(hm(18, 0), 0, 0));
    assert(!alertWindowContains(hm(18, 0), 0, hm(17, 59)));

    // --- one-minute window ---
    assert( alertWindowContains(hm(9, 30), hm(9, 31), hm(9, 30)));
    assert(!alertWindowContains(hm(9, 30), hm(9, 31), hm(9, 31)));

    // --- start == stop: no restriction (every minute of the day passes) ---
    for (int m = 0; m < ALERT_MINUTES_PER_DAY; ++m) assert(alertWindowContains(hm(9, 0), hm(9, 0), m));

    // --- every window shape: exactly (stop - start) mod 24h minutes are inside ---
    for (int a = 0; a < ALERT_MINUTES_PER_DAY; a += 37) {
        for (int b = 0; b < ALERT_MINUTES_PER_DAY; b += 41) {
            if (a == b) continue;
            int inside = 0;
            for (int m = 0; m < ALERT_MINUTES_PER_DAY; ++m) inside += alertWindowContains(a, b, m);
            assert(inside == ((b - a) + ALERT_MINUTES_PER_DAY) % ALERT_MINUTES_PER_DAY);
        }
    }

    // --- alertScheduleAllows: the gate used by checkAudioEvents() ---
    // off: always allowed, whatever the times say
    assert(alertScheduleAllows(false, s, e, true, hm(3, 0)));
    assert(alertScheduleAllows(false, s, e, true, hm(12, 0)));
    // on: only inside the window
    assert( alertScheduleAllows(true, s, e, true, hm(12, 0)));
    assert(!alertScheduleAllows(true, s, e, true, hm(3, 0)));
    assert(!alertScheduleAllows(true, ns, ne, true, hm(12, 0)));
    assert( alertScheduleAllows(true, ns, ne, true, hm(23, 0)));
    // on, but the clock isn't set (no NTP yet, no RTC time): fail open
    assert(alertScheduleAllows(true, s, e, false, hm(3, 0)));
    assert(alertScheduleAllows(true, s, e, false, 0));

    // --- the defaults the page offers are a valid, non-empty daytime window ---
    assert(ALERT_SCHED_START_DEFAULT_MIN >= 0 && ALERT_SCHED_START_DEFAULT_MIN < ALERT_MINUTES_PER_DAY);
    assert(ALERT_SCHED_STOP_DEFAULT_MIN  >= 0 && ALERT_SCHED_STOP_DEFAULT_MIN  < ALERT_MINUTES_PER_DAY);
    assert(ALERT_SCHED_START_DEFAULT_MIN != ALERT_SCHED_STOP_DEFAULT_MIN);
    assert(repairClockMinutes(ALERT_SCHED_START_DEFAULT_MIN, 0) == ALERT_SCHED_START_DEFAULT_MIN);
    assert(repairClockMinutes(ALERT_SCHED_STOP_DEFAULT_MIN, 0)  == ALERT_SCHED_STOP_DEFAULT_MIN);

    return 0;
}
