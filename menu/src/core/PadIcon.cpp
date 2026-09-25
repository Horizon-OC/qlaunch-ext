/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "PadIcon.hpp"
#include <switch.h>

namespace PadIcon {

static unsigned s_glyph = 0xE122;
static float s_r = 0.55f, s_g = 0.55f, s_b = 0.60f;
static unsigned s_tick = 0;
static bool s_done = false;

static void DecodeU32(float *r, float *g, float *b, u32 v)
{
    if (v == 0)
        return;
    float rr = (float)((v >> 24) & 0xFF) / 255.0f;
    float gg = (float)((v >> 16) & 0xFF) / 255.0f;
    float bb = (float)((v >> 8) & 0xFF) / 255.0f;

    *r = rr;
    *g = gg;
    *b = bb;
}

static void Refresh()
{
    u32 style = hidGetNpadStyleSet(HidNpadIdType_No1);
    if (style == 0)
        style = hidGetNpadStyleSet(HidNpadIdType_Handheld);

    unsigned glyph = 0xE122;
    if (style & HidNpadStyleTag_NpadHandheld) {
        glyph = 0xE121;
    } else if (style & HidNpadStyleTag_NpadJoyDual) {
        glyph = 0xE122;
    } else if (style & HidNpadStyleTag_NpadJoyLeft) {
        glyph = (style & HidNpadStyleTag_NpadJoyRight) ? 0xE122 : 0xE123;
    } else if (style & HidNpadStyleTag_NpadJoyRight) {
        glyph = 0xE124;
    } else if (style & HidNpadStyleTag_NpadFullKey) {
        glyph = 0xE12C;
    } else if (style & HidNpadStyleTag_NpadGc) {
        glyph = 0xE12C; /* No GC font */
    } else if (style & HidNpadStyleTag_NpadPalma) {
        glyph = 0xE122; /* No ball font */
    } else if (style & HidNpadStyleTag_NpadLark) {
        glyph = 0xE127;
    } else if (style & HidNpadStyleTag_NpadHandheldLark) {
        glyph = 0xE127;
    } else if (style & HidNpadStyleTag_NpadLucia) {
        glyph = 0xE128;
    } else if (style & HidNpadStyleTag_NpadLagon) {
        glyph = 0xE12C; /* No N64 glyph */
    } else if (style & HidNpadStyleTag_NpadLager) {
        glyph = 0xE129;
    } else if (style & (HidNpadStyleTag_NpadSystemExt | HidNpadStyleTag_NpadSystem)) {
        glyph = 0xE12C;
    } else if (style == 0) {
        glyph = 0xE122;
    }

    float r = 0.55f, g = 0.55f, b = 0.60f;
    HidNpadControllerColor left{}, right{}, single{};
    bool hasSplit = false, hasSingle = false;
    if (R_SUCCEEDED(hidGetNpadControllerColorSplit(HidNpadIdType_No1, &left, &right))) {
        if (left.main != 0 || right.main != 0)
            hasSplit = true;
    }
    if (R_SUCCEEDED(hidGetNpadControllerColorSingle(HidNpadIdType_No1, &single))) {
        if (single.main != 0)
            hasSingle = true;
    }
    if (hasSplit) {
        /* TODO: Do dual joycon properly */
        u32 v = left.main ? left.main : right.main;
        DecodeU32(&r, &g, &b, v);
    } else if (hasSingle) {
        DecodeU32(&r, &g, &b, single.main);
    }

    s_glyph = glyph;
    s_r = r;
    s_g = g;
    s_b = b;
    s_done = true;
}

void Tick()
{
    /* Poll colors slowly. */
    s_tick++;
    if (!s_done || (s_tick % 30) == 1)
        Refresh();
}

unsigned Glyph()
{
    if (!s_done)
        Refresh();
    return s_glyph;
}

void Color(float *r, float *g, float *b)
{
    if (!s_done)
        Refresh();
    if (r)
        *r = s_r;
    if (g)
        *g = s_g;
    if (b)
        *b = s_b;
}

} // namespace PadIcon