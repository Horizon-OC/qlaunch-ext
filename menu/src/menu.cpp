/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "core/App.hpp"

extern "C" {

int onBoot(NWindow *win)
{
    return App::Boot(win) ? 0 : -1;
}

int loop(void)
{
    App::Loop();
    return 0;
}

void onShutdown(void)
{
    App::Shutdown();
}

void onReload(void)
{
    App::OnReload();
}

void onPowerButton(void)
{
    App::OnPower();
}

void onOpen(void)
{
    App::OnOpen();
}

void onHomeButton(void)
{
    App::OnHome();
}

} /* extern C */

