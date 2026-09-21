/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

namespace sys {

bool HasForeground();
Result SetForeground();
void EnterSleep();

} // namespace sys
