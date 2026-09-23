/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>
#include <deko3d.h>

enum { ICONS_MAX = 65 };

struct IconReq {
    u64 tid;
    int rowIndex; /* row to write the slot into */
};

namespace Icons {
void Sync(const u64 *tids, int *slots, int n);
void FreeAll();

struct Entry {
    u64 tid;
    bool used;
    bool sweep;
    DkMemBlock mem;
    DkImage img;
};
bool Upload(Entry *e, const uint8_t *rgba, int w, int h);
void Free(Entry *e);
} /* namespace Icons */

