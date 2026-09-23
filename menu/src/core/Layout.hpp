/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

enum { LAYOUT_MAX_NODES = 48, LAYOUT_MAX_DOCK = 12, LAYOUT_MAX_HINTS = 8 };

struct DockItem {
    char glyph[16];
    char action[24];
};

struct HintItem {
    char button[8];
    char label[48];
};

struct LayoutNode {
    char type[16];
    char id[24];
    float x, y, tile, gap;
    bool label;
    DockItem dock[LAYOUT_MAX_DOCK];
    int dockCount;
    HintItem hints[LAYOUT_MAX_HINTS];
    int hintCount;
};

namespace Layout {
bool LoadAuto();
bool LoadJson(const char *json, unsigned len);
bool LoadBuiltin();
bool LoadFile(const char *path);
void Draw();
void Input(u64 down, u64 held);
void IconSlots(int *out);
void BeginIconSlots();
void PushIconSlot(int slot);
void ActivateDock();
int NodeCount();
const LayoutNode &Node(int i);
} /* namespace Layout */

