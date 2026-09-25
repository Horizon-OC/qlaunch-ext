/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */
#include "Icons.hpp"
#include "Gfx.hpp"
#include "Font.hpp"
#include "Theme.hpp"
#include "Qext.hpp"
#include <string.h>
#include <stdlib.h>

static void *stb_realloc_sized(void *p, size_t oldsz, size_t newsz)
{
    if (!p)
        return malloc(newsz);
    if (!newsz) {
        free(p);
        return NULL;
    }
    void *q = malloc(newsz);
    if (!q)
        return NULL;
    memcpy(q, p, oldsz < newsz ? oldsz : newsz);
    free(p);
    return q;
}
#define STBI_ASSERT(x)
#define STBI_MALLOC(sz) malloc(sz)
#define STBI_REALLOC_SIZED(p,oldsz,newsz) stb_realloc_sized(p,oldsz,newsz)
#define STBI_FREE(p) free(p)
#define STBI_NO_THREAD_LOCALS
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

extern const uint8_t hud_wifi_png[];
extern const uint32_t hud_wifi_png_size;
extern const uint8_t hud_battery_png[];
extern const uint32_t hud_battery_png_size;
extern const uint8_t hud_user_png[];
extern const uint32_t hud_user_png_size;
extern const uint8_t hud_btn_a_png[];
extern const uint32_t hud_btn_a_png_size;
extern const uint8_t hud_btn_plus_png[];
extern const uint32_t hud_btn_plus_png_size;
extern const uint8_t hud_folder_png[];
extern const uint32_t hud_folder_png_size;
extern const uint8_t hud_vgccard_png[];
extern const uint32_t hud_vgccard_png_size;
extern const uint8_t top_sel_home_png[];
extern const uint32_t top_sel_home_png_size;
extern const uint8_t top_sel_connect_png[];
extern const uint32_t top_sel_connect_png_size;
extern const uint8_t top_sel_eshop_png[];
extern const uint32_t top_sel_eshop_png_size;
extern const uint8_t top_sel_album_png[];
extern const uint32_t top_sel_album_png_size;
extern const uint8_t top_sel_settings_png[];
extern const uint32_t top_sel_settings_png_size;
extern const uint8_t top_des_home_png[];
extern const uint32_t top_des_home_png_size;
extern const uint8_t top_des_connect_png[];
extern const uint32_t top_des_connect_png_size;
extern const uint8_t top_des_eshop_png[];
extern const uint32_t top_des_eshop_png_size;
extern const uint8_t top_des_album_png[];
extern const uint32_t top_des_album_png_size;
extern const uint8_t top_des_settings_png[];
extern const uint32_t top_des_settings_png_size;
extern const uint8_t marker_sheet_png[];
extern const uint32_t marker_sheet_png_size;

namespace Icons {
static Entry entries_[ICONS_MAX];
static DkMemBlock hudMem_[HUD_COUNT] = { 0 };
static DkImage hudImg_[HUD_COUNT];
static bool hudOk_[HUD_COUNT] = { false };
static int hudTex_[HUD_COUNT] = { 0 };
} /* namespace Icons */

namespace {

/* NOTE: dont make these static pointer tables otherwise there will be crashes due to relocation stuff */
const uint8_t *HudPng(int i)
{
    switch (i) {
    case 1: return hud_battery_png;
    case 2: return hud_user_png;
    case 3: return hud_btn_a_png;
    case 4: return hud_btn_plus_png;
    case 5: return hud_folder_png;
    case 6: return hud_vgccard_png;
    case 7: return top_sel_home_png;
    case 8: return top_sel_connect_png;
    case 9: return top_sel_eshop_png;
    case 10: return top_sel_album_png;
    case 11: return top_sel_settings_png;
    case 12: return top_des_home_png;
    case 13: return top_des_connect_png;
    case 14: return top_des_eshop_png;
    case 15: return top_des_album_png;
    case 16: return top_des_settings_png;
    case 17: return marker_sheet_png;
    default: return hud_wifi_png;
    }
}

const uint32_t *HudPngSize(int i)
{
    switch (i) {
    case 1: return &hud_battery_png_size;
    case 2: return &hud_user_png_size;
    case 3: return &hud_btn_a_png_size;
    case 4: return &hud_btn_plus_png_size;
    case 5: return &hud_folder_png_size;
    case 6: return &hud_vgccard_png_size;
    case 7: return &top_sel_home_png_size;
    case 8: return &top_sel_connect_png_size;
    case 9: return &top_sel_eshop_png_size;
    case 10: return &top_sel_album_png_size;
    case 11: return &top_sel_settings_png_size;
    case 12: return &top_des_home_png_size;
    case 13: return &top_des_connect_png_size;
    case 14: return &top_des_eshop_png_size;
    case 15: return &top_des_album_png_size;
    case 16: return &top_des_settings_png_size;
    case 17: return &marker_sheet_png_size;
    default: return &hud_wifi_png_size;
    }
}

static uint8_t *Downscale(uint8_t *px, int *w, int *h)
{
    int sw = *w, sh = *h;
    int dw = sw, dh = sh;
    while (dw > 192 || dh > 192) {
        dw /= 2;
        dh /= 2;
    }
    if (dw == sw && dh == sh)
        return px;
    uint8_t *d = (uint8_t *)malloc((size_t)dw * (size_t)dh * 4);
    if (!d)
        return px;
    for (int y = 0; y < dh; y++) {
        int y0 = y * sh / dh;
        int y1 = (y + 1) * sh / dh;
        if (y1 <= y0)
            y1 = y0 + 1;
        for (int x = 0; x < dw; x++) {
            int x0 = x * sw / dw;
            int x1 = (x + 1) * sw / dw;
            if (x1 <= x0)
                x1 = x0 + 1;
            unsigned sr = 0, sg = 0, sb = 0, sa = 0;
            unsigned cnt = 0;
            for (int sy = y0; sy < y1; sy++) {
                for (int sx = x0; sx < x1; sx++) {
                    uint8_t *p = px + ((size_t)sy * (size_t)sw + (size_t)sx) * 4;
                    sr += p[0];
                    sg += p[1];
                    sb += p[2];
                    sa += p[3];
                    cnt++;
                }
            }
            uint8_t *o = d + ((size_t)y * (size_t)dw + (size_t)x) * 4;
            o[0] = (uint8_t)(sr / cnt);
            o[1] = (uint8_t)(sg / cnt);
            o[2] = (uint8_t)(sb / cnt);
            o[3] = (uint8_t)(sa / cnt);
        }
    }
    stbi_image_free(px);
    *w = dw;
    *h = dh;
    return d;
}

static bool HudNoInvert(int i)
{
    return i == HUD_BATTERY;
}

} // namespace

bool Icons::InitHud()
{
    bool any = false;
    for (int i = 0; i < HUD_COUNT; i++) {
        if (hudOk_[i])
            continue;

        int w = 0, h = 0;
        uint8_t *px = stbi_load_from_memory(HudPng(i), (int)*HudPngSize(i),
                                            &w, &h, NULL, 4);
        if (px && (w > 256 || h > 256) && i != HUD_MARKER_STRIP)
            px = Downscale(px, &w, &h);
        if (px && Theme::IsDark() && !HudNoInvert(i)) {
            unsigned long long lum = 0;
            unsigned cnt = 0;
            for (int pxi = 0; pxi < w * h; pxi++) {
                if (px[pxi * 4 + 3] > 8) {
                    lum += (unsigned)px[pxi * 4] * 30u +
                           (unsigned)px[pxi * 4 + 1] * 59u +
                           (unsigned)px[pxi * 4 + 2] * 11u;
                    cnt++;
                }
            }
            if (cnt > 0 && lum / cnt < 128u * 100u) {
                for (int pxi = 0; pxi < w * h; pxi++) {
                    px[pxi * 4] = (uint8_t)(255 - px[pxi * 4]);
                    px[pxi * 4 + 1] = (uint8_t)(255 - px[pxi * 4 + 1]);
                    px[pxi * 4 + 2] = (uint8_t)(255 - px[pxi * 4 + 2]);
                }
            }
        }
        if (!px || w <= 0 || h <= 0 || (w > 512 && i != HUD_MARKER_STRIP) || h > 4096) {
            if (px)
                stbi_image_free(px);
            continue;
        }
        DkDevice dev = Gfx::Device();
        DkImageLayoutMaker lm;
        dkImageLayoutMakerDefaults(&lm, dev);
        lm.flags = DkImageFlags_PitchLinear;
        lm.format = DkImageFormat_RGBA8_Unorm;
        lm.dimensions[0] = (uint32_t)w;
        lm.dimensions[1] = (uint32_t)h;
        DkImageLayout lay;
        dkImageLayoutInitialize(&lay, &lm);
        uint32_t sz = dkImageLayoutGetSize(&lay);
        uint32_t al = dkImageLayoutGetAlignment(&lay);
        sz = (sz + al - 1) & ~(al - 1);
        sz = (sz + 0xFFFu) & ~0xFFFu;
        DkMemBlockMaker mm;
        dkMemBlockMakerDefaults(&mm, dev, sz);
        mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
        DkMemBlock mem = dkMemBlockCreate(&mm);
        if (!mem) {
            stbi_image_free(px);
            continue;
        }
    {
        uint32_t rowBytes = (uint32_t)w * 4u;
        uint32_t imgSize = dkImageLayoutGetSize(&lay);
        uint32_t pitch = h > 0 ? imgSize / (uint32_t)h : 0;
        uint8_t *dst = (uint8_t *)dkMemBlockGetCpuAddr(mem);
        if (pitch == rowBytes) {
            memcpy(dst, px, (size_t)rowBytes * (size_t)h);
        } else if (pitch > rowBytes && imgSize == pitch * (uint32_t)h) {
            for (int yy = 0; yy < h; yy++)
                memcpy(dst + (size_t)yy * pitch, px + (size_t)yy * rowBytes, rowBytes);
        } else {
            memcpy(dst, px, (size_t)rowBytes * (size_t)h);
        }
    }
        stbi_image_free(px);
        if (hudTex_[i] <= 2) {
            hudTex_[i] = Gfx::TexAlloc();
            if (hudTex_[i] < 0) {
                stbi_image_free(px);
                dkMemBlockDestroy(mem);
                continue;
            }
        }
        dkImageInitialize(&hudImg_[i], &lay, mem, 0);
        hudMem_[i] = mem;
        DkImageView view;
        dkImageViewDefaults(&view, &hudImg_[i]);
        Gfx::WriteImageDesc(hudTex_[i], &view);
        hudOk_[i] = true;
        any = true;
    }
    Gfx::WriteSamplerDesc();
    return any;
}

void Icons::ResetHud()
{
    for (int i = 0; i < HUD_COUNT; i++) {
        if (hudMem_[i]) {
            Gfx::WaitIdle();
            dkMemBlockDestroy(hudMem_[i]);
            hudMem_[i] = 0;
            hudOk_[i] = false;
        }
        Gfx::TexFree(hudTex_[i]);
        hudTex_[i] = -1;
    }
}

int Icons::HudSlot(int which)
{
    if (which < 0 || which >= HUD_COUNT || !hudOk_[which])
        return -1;
    return hudTex_[which];
}

bool Icons::Upload(Icons::Entry *e, const uint8_t *rgba, int w, int h)
{
    DkDevice dev = Gfx::Device();
    DkImageLayoutMaker lm;
    dkImageLayoutMakerDefaults(&lm, dev);
    lm.flags = DkImageFlags_PitchLinear;
    lm.format = DkImageFormat_RGBA8_Unorm;
    lm.dimensions[0] = (uint32_t)w;
    lm.dimensions[1] = (uint32_t)h;
    DkImageLayout lay;
    dkImageLayoutInitialize(&lay, &lm);
    uint32_t sz = dkImageLayoutGetSize(&lay);
    uint32_t al = dkImageLayoutGetAlignment(&lay);
    sz = (sz + al - 1) & ~(al - 1);
    sz = (sz + 0xFFFu) & ~0xFFFu; /* neko3d needs 4K-multiple blocks */

    DkMemBlockMaker mm;
    dkMemBlockMakerDefaults(&mm, dev, sz);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    DkMemBlock mem = dkMemBlockCreate(&mm);
    if (!mem)
        return false;

    {
        uint32_t rowBytes = (uint32_t)w * 4u;
        uint32_t imgSize = dkImageLayoutGetSize(&lay);
        uint32_t pitch = h > 0 ? imgSize / (uint32_t)h : 0;
        uint8_t *dst = (uint8_t *)dkMemBlockGetCpuAddr(mem);
        if (pitch == rowBytes) {
            memcpy(dst, rgba, (size_t)rowBytes * (size_t)h);
        } else if (pitch > rowBytes && imgSize == pitch * (uint32_t)h) {
            for (int yy = 0; yy < h; yy++)
                memcpy(dst + (size_t)yy * pitch, rgba + (size_t)yy * rowBytes, rowBytes);
        } else {
            memcpy(dst, rgba, (size_t)rowBytes * (size_t)h);
        }
    }
    dkImageInitialize(&e->img, &lay, mem, 0);
    e->mem = mem;
    return true;
}

void Icons::Free(Icons::Entry *e)
{
    if (e->used) {
        Gfx::WaitIdle();
        dkMemBlockDestroy(e->mem);
        e->used = false;
        e->tid = 0;
    }
    Gfx::TexFree(e->slot);
    e->slot = -1;
}

void Icons::FreeAll()
{
    for (int i = 0; i < ICONS_MAX; i++)
        Free(&entries_[i]);
    for (int i = 0; i < HUD_COUNT; i++) {
        if (hudMem_[i]) {
            dkMemBlockDestroy(hudMem_[i]);
            hudMem_[i] = 0;
            hudOk_[i] = false;
        }
        Gfx::TexFree(hudTex_[i]);
        hudTex_[i] = -1;
    }
}

void Icons::Sync(const u64 *tids, int *slots, int n)
{
    for (int i = 0; i < n; i++) {
        if (!tids[i])
            continue;
        bool have = false;
        for (int k = 0; k < ICONS_MAX; k++) {
            if (entries_[k].used && entries_[k].tid == tids[i]) {
                have = true;
                break;
            }
        }
        if (!have) {
            Gfx::WaitIdle();
            break;
        }
    }
    for (int i = 0; i < ICONS_MAX; i++)
        entries_[i].sweep = false;
    for (int i = 0; i < n; i++) {
        u64 tid = tids[i];
        slots[i] = -1;
        if (!tid)
            continue;
        int found = -1;
        for (int k = 0; k < ICONS_MAX; k++) {
            if (entries_[k].used && entries_[k].tid == tid) {
                found = k;
                break;
            }
        }
        if (found < 0) {
            for (int k = 0; k < ICONS_MAX; k++) {
                if (!entries_[k].used) {
                    found = k;
                    break;
                }
            }
        }
        if (found < 0)
            continue;
        Entry *e = &entries_[found];
        if (!e->used) {
            bool ok = false;
            int count = qext_title_count();
            for (int ti = 0; ti < count; ti++) {
                if (qext_title_id(ti) != tid)
                    continue;
                int jsize = qext_title_icon_size(ti);
                if (jsize <= 0 || jsize > 0x40000)
                    break;
                uint8_t head[4] = { 0 };
                uint8_t *jpg = (uint8_t *)malloc((unsigned)jsize);
                if (!jpg)
                    break;
                int got = qext_title_icon(ti, jpg, (unsigned)jsize);
                if (got > 2 && jpg[0] == 0xFF && jpg[1] == 0xD8) {
                    int w = 0, h = 0;
                    uint8_t *px = stbi_load_from_memory(
                        jpg, got, &w, &h, NULL, 4);
                    if (px && w > 0 && h > 0 && w <= 1024 && h <= 1024) {
                        if (e->slot <= 2)
                            e->slot = Gfx::TexAlloc();
                        if (e->slot >= 3 && Upload(e, px, w, h)) {
                            e->tid = tid;
                            e->used = true;
                            DkImageView uv;
                            dkImageViewDefaults(&uv, &e->img);
                            Gfx::WriteImageDesc(e->slot, &uv);
                            ok = true;
                        } else {
                            Gfx::TexFree(e->slot);
                            e->slot = -1;
                        }
                    }
                    if (px)
                        stbi_image_free(px);
                }
                free(jpg);
                break;
            }
            if (!ok)
                continue;
        }
        e->sweep = true;
        slots[i] = e->slot;
    }

    for (int k = 0; k < ICONS_MAX; k++) {
        if (entries_[k].used && !entries_[k].sweep)
            Free(&entries_[k]);
    }
    (void)n;
}

