/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Clock.hpp"
#include "Qext.hpp"
#include <string.h>
#include <stdio.h>

namespace Clock {

static char s_clock[40] = "--:--"; static char s_time[16] = "--:--"; static char s_ampm[4] = "--";
static char s_date[16] = "--/--";
static unsigned s_batt = 100;

void Tick()
{
    u64 ts = 0;
    TimeCalendarTime cal = { 0 };
    TimeCalendarAdditionalInfo info = { 0 };
    if (R_SUCCEEDED(timeGetCurrentTime(TimeType_UserSystemClock, &ts)) &&
        R_SUCCEEDED(timeToCalendarTimeWithMyRule(ts, &cal, &info)) &&
        cal.year >= 2000 && cal.month >= 1 && cal.month <= 12 &&
        cal.day >= 1 && cal.day <= 31) {
        int h12 = (int)(cal.hour % 12);
        if (h12 == 0)
            h12 = 12;
        const char *ap = (cal.hour < 12) ? "AM" : "PM";
        snprintf(s_clock, sizeof(s_clock), "%d:%02d %s", h12, (int)cal.minute, ap);
        snprintf(s_time, sizeof(s_time), "%d:%02d", h12, (int)cal.minute);
        snprintf(s_ampm, sizeof(s_ampm), "%s", ap);
        snprintf(s_date, sizeof(s_date), "%02d/%02d",
                 (int)cal.month, (int)cal.day);
    } else {
        memcpy(s_clock, "--:--", 6); memcpy(s_time, "--:--", 6); memcpy(s_ampm, "--", 3);
        memcpy(s_date, "--/--", 6);
    }
    u32 pct = 100;
    if (R_SUCCEEDED(psmGetBatteryChargePercentage(&pct)) && pct <= 100)
        s_batt = pct;
}

const char *Str() { return s_clock; } const char *TimeStr() { return s_time; } const char *AmPm() { return s_ampm; }
const char *Date() { return s_date; }
unsigned Batt() { return s_batt; }

} /* namespace Clock */

