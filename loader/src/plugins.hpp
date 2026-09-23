/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering...         */

#pragma once
#include <switch.h>

namespace plugins {

void Boot(NWindow *win);

void Loop(bool inGame);

void OpenAll();
void PowerAll();

void Shutdown();

} /* namespace plugins */

