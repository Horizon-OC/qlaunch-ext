/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */
#include "Icons.hpp"
#include "Gfx.hpp"
#include "Font.hpp"
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

namespace Icons {
static Entry entries_[ICONS_MAX];
} /* namespace Icons */

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
    memcpy(dkMemBlockGetCpuAddr(mem), rgba, (size_t)w * (size_t)h * 4);
    dkImageInitialize(&e->img, &lay, mem, 0);
    e->mem = mem;
    return true;
}

void Icons::Free(Icons::Entry *e)
{
    if (e->used) {
        dkMemBlockDestroy(e->mem);
        e->used = false;
        e->tid = 0;
    }
}

void Icons::FreeAll()
{
    for (int i = 0; i < ICONS_MAX; i++)
        Free(&entries_[i]);
}

void Icons::Sync(const u64 *tids, int *slots, int n)
{
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
                        if (Upload(e, px, w, h)) {
                            e->tid = tid;
                            e->used = true;
                            ok = true;
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
        slots[i] = found + ICONS_FIRST_TEX;
    }
    DkImageView view;
    int kept = 0;
    for (int k = 0; k < ICONS_MAX; k++) {
        if (entries_[k].used && !entries_[k].sweep)
            Free(&entries_[k]);
        if (entries_[k].used) {
            dkImageViewDefaults(&view, &entries_[k].img);
            Gfx::WriteImageDesc(k + ICONS_FIRST_TEX, &view);
            kept++;
        }
    }
    (void)n;
    (void)kept;
}

