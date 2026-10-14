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

/* Called once after load. Return 0 to stay loaded, non-zero to unload.
 * Exporting loop is mandatory; a plugin without it is skipped. */
QLAUNCH_HOOK int onBoot(NWindow *win);

/* Called every loader tick (skipped in-game unless IsPersistent).
 * Return non-zero to unload yourself. */
QLAUNCH_HOOK int loop(void);

/* Called before unload and on loader shutdown. */
QLAUNCH_HOOK void onShutdown(void);

/* Called when the menu opens. Optional. */
QLAUNCH_HOOK void onOpen(void);

/* Called on power-button press. Optional. */
QLAUNCH_HOOK void onPowerButton(void);

/* True when loop() must also run while a game is in the foreground.
 * Optional (defaults to false). */
QLAUNCH_HOOK bool IsPersistent(void);

#ifdef __cplusplus
} /* extern C */
#endif
