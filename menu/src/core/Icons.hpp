/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>
#include <deko3d.h>

enum { ICONS_MAX = 45 };

enum {
    HUD_WIFI = 0,
    HUD_BATTERY,
    HUD_USER,
    HUD_BTN_A,
    HUD_BTN_PLUS,
    HUD_FOLDER,
    HUD_VGCCARD,
    HUD_TOPSEL_HOME,
    HUD_TOPSEL_CONNECT,
    HUD_TOPSEL_ESHOP,
    HUD_TOPSEL_ALBUM,
    HUD_TOPSEL_SETTINGS,
    HUD_TOPDES_HOME,
    HUD_TOPDES_CONNECT,
    HUD_TOPDES_ESHOP,
    HUD_TOPDES_ALBUM,
    HUD_TOPDES_SETTINGS,
    HUD_MARKER_STRIP,
    HUD_COUNT
};

struct IconReq {
    u64 tid;
    int rowIndex; /* row to write the slot into */
};

namespace Icons {
void Sync(const u64 *tids, int *slots, int n);
void FreeAll();
void ResetHud();
bool InitHud();
int HudSlot(int which);

struct Entry {
    u64 tid;
    bool used;
    bool sweep;
    int slot;
    DkMemBlock mem;
    DkImage img;
};
bool Upload(Entry *e, const uint8_t *rgba, int w, int h);
void Free(Entry *e);
} /* namespace Icons */

