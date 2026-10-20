/* qlaunch-ext plugin SDK (C) 2026 Souldbminer */
/* Licensed under the GPLv2                  */

#pragma once

/* Export marker for hook symbols */
#ifndef QLAUNCH_HOOK
#define QLAUNCH_HOOK __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /* Called once on load. Return 0 to stay loaded.
       Gives the menu a window for drawing          */
    QLAUNCH_HOOK int onBoot(NWindow *win);

    /* Called every tick in loader (not in game unless you mark yourself as persistent)
    * Return non-zero to unload yourself. */
    QLAUNCH_HOOK int loop(void);

    /* Called before unload and on loader shutdown. */
    QLAUNCH_HOOK void onShutdown(void);

    /* Called when the menu opens. Optional. */
    QLAUNCH_HOOK void onOpen(void);

    /* Called on power-button press. Optional. */
    QLAUNCH_HOOK void onPowerButton(void);

    /* If you return true you run while in a game (marked as persistent) */
    QLAUNCH_HOOK bool IsPersistent(void);

#ifdef __cplusplus
} /* extern C */
#endif
