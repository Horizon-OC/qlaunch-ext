/* qlaunch-ext TitlePlugin SDK (C) 2026 Souldbminer */
/* Licensed under the GPLv2                          */
/* Lets a plugin inject virtual titles */

#pragma once

#ifndef QLAUNCH_HOOK
#define QLAUNCH_HOOK __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    /* Number of virtual titles right now (0 = none for now). */
    QLAUNCH_HOOK int TitleCount(void);

    /* Fake TID for the title. Optional, but reccomended. Must have upper high bit set */
    QLAUNCH_HOOK unsigned long long TitleTid(int index);

    /* Donor title ID */
    QLAUNCH_HOOK unsigned long long TitleDonor(int index);

    /* The NRO to boot */
    QLAUNCH_HOOK int TitleNro(int index, char *out, unsigned cap);

    /* Name length into out (<0 on error). */
    QLAUNCH_HOOK int TitleName(int index, char *out, unsigned cap);

    /* JPEG size for menu title */
    QLAUNCH_HOOK int TitleIconSize(int index);

    /* JPEG bytes. Returns Bytes written
    * (<0 on error). Optional. */
    QLAUNCH_HOOK int TitleIcon(int index, void *out, unsigned cap);

    /* Display version string into out (<0 on error/unknown). Optional. */
    QLAUNCH_HOOK int TitleVersion(int index, char *out, unsigned cap);

    /* Publisher string into out (<0 on error/unknown). Optional. */
    QLAUNCH_HOOK int TitlePublisher(int index, char *out, unsigned cap);

    /* Called on launch of any title. */
    QLAUNCH_HOOK int TitleLaunch(int index);

    /* Called on title refresh. */
    QLAUNCH_HOOK void TitleRefresh(void);

#ifdef __cplusplus
} /* extern C */
#endif
