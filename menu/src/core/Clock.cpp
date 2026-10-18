/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Clock.hpp"
#include "Qext.hpp"
#include <string.h>
#include <stdio.h>

namespace Clock {

    namespace {

        /// @brief All mutable clock cache state, single global.
        struct ClockState {
            char clockText[40] = "--:--";
            char timeText[16] = "--:--";
            char meridiemText[4] = "--";
            char dateText[16] = "--/--";
            unsigned batteryPercent = 100;
            int chargingState = -1;
            int wifiSignal = -1;
            int connectionType = -1;
            int airplaneMode = -1;
        };

        static ClockState s_state = {};

    } // namespace

    /// @brief Poll system time and battery state into the cache.
    void Tick()
    {
        u64 timestampSeconds = 0;
        TimeCalendarTime calendarTime = { 0 };
        TimeCalendarAdditionalInfo calendarInfo = { 0 };

        if (R_SUCCEEDED(qext_time_now(&timestampSeconds)) &&
            R_SUCCEEDED(qext_time_calendar(timestampSeconds, &calendarTime, &calendarInfo)) &&
            calendarTime.year >= 2000 && calendarTime.month >= 1 && calendarTime.month <= 12 &&
            calendarTime.day >= 1 && calendarTime.day <= 31) {
            int twelveHour = (int)(calendarTime.hour % 12);

            if (twelveHour == 0) {
                twelveHour = 12;
            }

            const char *meridiemLabel = (calendarTime.hour < 12) ? "AM" : "PM";
            snprintf(s_state.clockText, sizeof(s_state.clockText), "%d:%02d %s", twelveHour, (int)calendarTime.minute, meridiemLabel);
            snprintf(s_state.timeText, sizeof(s_state.timeText), "%d:%02d", twelveHour, (int)calendarTime.minute);
            snprintf(s_state.meridiemText, sizeof(s_state.meridiemText), "%s", meridiemLabel);
            snprintf(s_state.dateText, sizeof(s_state.dateText), "%02d/%02d",
                     (int)calendarTime.month, (int)calendarTime.day);
        } else {
            memcpy(s_state.clockText, "--:--", 6);
            memcpy(s_state.timeText, "--:--", 6);
            memcpy(s_state.meridiemText, "--", 3);
            memcpy(s_state.dateText, "--/--", 6);
        }

        int batteryPercent = qext_battery_percent();

        if (batteryPercent >= 0 && batteryPercent <= 100) {
            s_state.batteryPercent = (unsigned)batteryPercent;
        }

        int chargingRaw = qext_battery_charging();
        s_state.chargingState = (chargingRaw < 0) ? -1 : (chargingRaw > 0 ? 1 : 0);

        int wifiRaw = qext_wifi_signal();
        s_state.wifiSignal = (wifiRaw < -1 || wifiRaw > 3) ? -1 : wifiRaw;

        int connectionRaw = qext_connection_type();
        s_state.connectionType = (connectionRaw < 0 || connectionRaw > 2) ? -1 : connectionRaw;

        int airplaneRaw = qext_airplane_mode();
        s_state.airplaneMode = (airplaneRaw < 0) ? -1 : (airplaneRaw > 0 ? 1 : 0);
    }

    /// @brief Give the cached date/time string.
    /// @return Date/time text.
    const char *Str()
    {
        return s_state.clockText;
    }

    /// @brief Give the cached time string ("h:mm").
    /// @return Time text.
    const char *TimeStr()
    {
        return s_state.timeText;
    }

    /// @brief Give the cached meridiem ("AM"/"PM").
    /// @return Meridiem text.
    const char *AmPm()
    {
        return s_state.meridiemText;
    }

    /// @brief Give the cached date string.
    /// @return Date text.
    const char *Date()
    {
        return s_state.dateText;
    }

    /// @brief Give the cached battery percentage.
    /// @return 0-100, or >100 when unknown.
    unsigned Batt()
    {
        return s_state.batteryPercent;
    }

    /// @brief Whether the battery is charging.
    /// @return 1 when charging, 0 when not, < 0 when unknown.
    int Charging()
    {
        return s_state.chargingState;
    }

    /// @brief Give the cached WiFi signal level.
    /// @return 0 when not connected, 1-3 bars when connected,
    /// < 0 when unknown.
    int Wifi()
    {
        return s_state.wifiSignal;
    }

    /// @brief Give the cached connection type.
    /// @return QEXT_NET_* code, or < 0 when unknown.
    int NetType()
    {
        return s_state.connectionType;
    }

    /// @brief Whether airplane mode is on.
    /// @return 1 when on, 0 when off, < 0 when unknown.
    int Plane()
    {
        return s_state.airplaneMode;
    }

} /* namespace Clock */
