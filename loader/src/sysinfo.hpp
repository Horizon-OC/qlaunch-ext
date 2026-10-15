/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

namespace sysinfo {

/* Raw system information for menus and plugins. Services are owned
   by the loader; these return data only. Formatting, decoding and
   drawing stay on the caller side. */

/* Lists user ids (up to cap) into out.
   Returns the total user count, or < 0 on failure. */
int AccountList(AccountUid *out, int cap);

/* Gives a user's profile JPEG byte size.
   Returns the size, or < 0 when missing/unusable. */
int AccountImageSize(AccountUid uid);

/* Copies a user's profile JPEG bytes.
   Returns bytes written (<0 on error). */
int AccountImage(AccountUid uid, void *out, unsigned cap);

/* Reads the user system clock.
   Returns a libnx result code. */
Result TimeNow(u64 *ts);

/* Converts a timestamp to calendar time with the user rule.
   Returns a libnx result code. */
Result TimeCalendar(u64 ts, TimeCalendarTime *cal, TimeCalendarAdditionalInfo *info);

/* Gives the battery charge percentage.
   Returns 0-100, or < 0 when unknown. */
int BatteryPercent();

/* Gives the system color-set id, defaulting to light.
   Returns the active color set. */
ColorSetId ColorSet();

/* Tests whether a user id is valid (non-empty).
   Returns non-zero when valid. */
int AccountUidValid(AccountUid uid);

} // namespace sysinfo
