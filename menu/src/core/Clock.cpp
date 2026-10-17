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
static int s_charging = -1;
static int s_wifi = -1;
static int s_net = -1;
static int s_plane = -1;

    /// @brief Poll system time and battery state into the cache.
    void Tick()
{
    u64 ts = 0;
    TimeCalendarTime cal = { 0 };
    TimeCalendarAdditionalInfo info = { 0 };
    if (R_SUCCEEDED(qext_time_now(&ts)) &&
        R_SUCCEEDED(qext_time_calendar(ts, &cal, &info)) &&
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
    int pct = qext_battery_percent();
    if (pct >= 0 && pct <= 100)
        s_batt = (unsigned)pct;
    int chg = qext_battery_charging();
    s_charging = (chg < 0) ? -1 : (chg > 0 ? 1 : 0);
    int sig = qext_wifi_signal();
    s_wifi = (sig < -1 || sig > 3) ? -1 : sig;
    int net = qext_connection_type();
    s_net = (net < 0 || net > 2) ? -1 : net;
    int pl = qext_airplane_mode();
    s_plane = (pl < 0) ? -1 : (pl > 0 ? 1 : 0);
}

    /// @brief Give the cached date/time string.
    /// @return Date/time text.
    const char *Str() { return s_clock; }

    /// @brief Give the cached time string ("h:mm").
    /// @return Time text.
    const char *TimeStr() { return s_time; }

    /// @brief Give the cached meridiem ("AM"/"PM").
    /// @return Meridiem text.
    const char *AmPm() { return s_ampm; }

    /// @brief Give the cached date string.
    /// @return Date text.
    const char *Date() { return s_date; }

    /// @brief Give the cached battery percentage.
    /// @return 0-100, or >100 when unknown.
    unsigned Batt() { return s_batt; }

    /// @brief Whether the battery is charging.
    /// @return 1 when charging, 0 when not, < 0 when unknown.
    int Charging() { return s_charging; }

    /// @brief Give the cached WiFi signal level.
    /// @return 0 when not connected, 1-3 bars when connected,
    /// < 0 when unknown.
    int Wifi() { return s_wifi; }

    /// @brief Give the cached connection type.
    /// @return QEXT_NET_* code, or < 0 when unknown.
    int NetType() { return s_net; }

    /// @brief Whether airplane mode is on.
    /// @return 1 when on, 0 when off, < 0 when unknown.
    int Plane() { return s_plane; }

} /* namespace Clock */

