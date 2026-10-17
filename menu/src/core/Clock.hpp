/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

namespace Clock {
    /// @brief Poll system time and battery state into the cache.
    void Tick();

    /// @brief Give the cached date/time string.
    /// @return Date/time text.
    const char *Str();

    /// @brief Give the cached time string ("h:mm").
    /// @return Time text.
    const char *TimeStr(); /* "h:mm" */

    /// @brief Give the cached meridiem ("AM"/"PM").
    /// @return Meridiem text.
    const char *AmPm();    /* "AM"/"PM" */

    /// @brief Give the cached date string.
    /// @return Date text.
    const char *Date();

    /// @brief Give the cached battery percentage.
    /// @return 0-100, or >100 when unknown.
    unsigned Batt();

    /// @brief Whether the battery is charging.
    /// @return 1 when charging, 0 when not, < 0 when unknown.
    int Charging();

    /// @brief Give the cached WiFi signal level.
    /// @return 0 when not connected, 1-3 bars when connected,
    /// < 0 when unknown.
    int Wifi();

    /// @brief Give the cached connection type.
    /// @return QEXT_NET_* code, or < 0 when unknown.
    int NetType();

    /// @brief Whether airplane mode is on.
    /// @return 1 when on, 0 when off, < 0 when unknown.
    int Plane();
} /* namespace Clock */
