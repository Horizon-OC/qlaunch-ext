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
} /* namespace Clock */
