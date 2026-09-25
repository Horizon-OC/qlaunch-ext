/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

namespace Clock {
void Tick();
const char *Str();
const char *TimeStr(); /* "h:mm" */
const char *AmPm();    /* "AM"/"PM" */
const char *Date();
unsigned Batt();
} /* namespace Clock */