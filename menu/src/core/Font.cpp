/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Font.hpp"
#include "Gfx.hpp"
#include "Log.hpp"
#include <string.h>

#define STBTT_assert(x)
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

extern const uint8_t font_otf[];
extern const uint32_t font_otf_size;
extern const uint8_t switch_icons_ttf[];
extern const uint32_t switch_icons_ttf_size;

namespace Font {

struct Atlas {
    int first;
    int count;
    int slot;
    stbtt_bakedchar g[512];
    float px;
    DkMemBlock mem;
    DkImage img;
    DkImageView view;
    bool ok;
};

static uint8_t s_scratch[FONT_ATLAS_W * FONT_ATLAS_H];
static Atlas s_at[3];
static bool s_ok;

static bool BakeOne(Atlas *a, const uint8_t *file, const float *heights,
                    int nHeights)
{
    for (int i = 0; i < nHeights; i++) {
        memset(s_scratch, 0, sizeof(s_scratch));
        int rows = stbtt_BakeFontBitmap(file, 0, heights[i], s_scratch,
            FONT_ATLAS_W, FONT_ATLAS_H, a->first, a->count, a->g);
        Log::Line("[menu] bake first=0x%X px=%.0f rows=%d", (unsigned)a->first,
                  heights[i], rows);
        if (rows > 0 && rows <= FONT_ATLAS_H) {
            a->px = heights[i];
            return true;
        }
    }
    a->px = 0.0f;
    return false;
}

static bool UploadOne(Atlas *a)
{
    DkDevice dev = Gfx::Device();
    DkImageLayoutMaker lm;
    dkImageLayoutMakerDefaults(&lm, dev);
    lm.flags = DkImageFlags_PitchLinear;
    lm.format = DkImageFormat_R8_Unorm;
    lm.dimensions[0] = FONT_ATLAS_W;
    lm.dimensions[1] = FONT_ATLAS_H;
    DkImageLayout lay;
    dkImageLayoutInitialize(&lay, &lm);
    uint32_t sz = dkImageLayoutGetSize(&lay);
    uint32_t al = dkImageLayoutGetAlignment(&lay);
    sz = (sz + al - 1) & ~(al - 1);
    sz = (sz + 0xFFFu) & ~0xFFFu; /* neko3d needs 4K-multiple blocks */

    DkMemBlockMaker mm;
    dkMemBlockMakerDefaults(&mm, dev, sz);
    mm.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
    a->mem = dkMemBlockCreate(&mm);
    if (!a->mem)
        return false;
    memcpy(dkMemBlockGetCpuAddr(a->mem), s_scratch,
           (size_t)FONT_ATLAS_W * (size_t)FONT_ATLAS_H);
    dkImageInitialize(&a->img, &lay, a->mem, 0);

    dkImageViewDefaults(&a->view, &a->img);
    a->view.swizzle[0] = DkImageSwizzle_One;
    a->view.swizzle[1] = DkImageSwizzle_One;
    a->view.swizzle[2] = DkImageSwizzle_One;
    a->view.swizzle[3] = DkImageSwizzle_Red;
    Gfx::WriteImageDesc(a->slot, &a->view);
    a->ok = true;
    return true;
}

void Shutdown()
{
    if (!s_ok)
        return;
    s_ok = false;
    for (int i = 0; i < 3; i++) {
        if (s_at[i].ok) {
            s_at[i].ok = false;
            if (s_at[i].mem)
                dkMemBlockDestroy(s_at[i].mem);
            s_at[i].mem = 0;
        }
    }
}

bool Init()
{
    static const float big[] = { 64.0f, 56.0f, 48.0f, 40.0f };
    static const float sml[] = { 48.0f, 40.0f, 32.0f };
    s_at[0].first = 32;
    s_at[0].count = 224;
    s_at[0].slot = FONT_SLOT_LATIN;
    s_at[0].ok = false;
    s_at[1].first = 0x3040;
    s_at[1].count = 192;
    s_at[1].slot = FONT_SLOT_KANA;
    s_at[1].ok = false;
    s_at[2].first = 0xE000;
    s_at[2].count = 512;
    s_at[2].slot = FONT_SLOT_ICON;
    s_at[2].ok = false;
    if (!BakeOne(&s_at[0], font_otf, big, 4))
        return false;
    if (!UploadOne(&s_at[0]))
        return false;
    if (BakeOne(&s_at[1], font_otf, big, 4))
        UploadOne(&s_at[1]);
    if (BakeOne(&s_at[2], switch_icons_ttf, sml, 3))
        UploadOne(&s_at[2]);
    Gfx::WriteSamplerDesc();
    s_ok = true;
    return true;
}

static bool RebakeOne(Atlas *a, const uint8_t *file, const float *heights,
                    int nHeights)
{
    if (!BakeOne(a, file, heights, nHeights))
        return false;
    memcpy(dkMemBlockGetCpuAddr(a->mem), s_scratch,
           (size_t)FONT_ATLAS_W * (size_t)FONT_ATLAS_H);
    Gfx::WriteImageDesc(a->slot, &a->view);
    return true;
}

void Refresh()
{
    if (!s_ok)
        return;
    static const float big[] = { 64.0f, 56.0f, 48.0f, 40.0f };
    static const float sml[] = { 48.0f, 40.0f, 32.0f };
    RebakeOne(&s_at[0], font_otf, big, 4);
    RebakeOne(&s_at[1], font_otf, big, 4);
    RebakeOne(&s_at[2], switch_icons_ttf, sml, 3);
    Gfx::WriteSamplerDesc();
}

bool Ok()
{
    return s_ok;
}

unsigned Next(const char **pp)
{
    const uint8_t *p = (const uint8_t *)*pp;
    uint8_t c = *p;
    if (c < 0x80) {
        *pp = (const char *)(p + 1);
        return c;
    }
    unsigned cp;
    int need;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; need = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; need = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; need = 3; }
    else { *pp = (const char *)(p + 1); return 0xFFFD; }
    for (int i = 0; i < need; i++) {
        uint8_t d = p[1 + i];
        if ((d & 0xC0) != 0x80) {
            *pp = (const char *)(p + 1);
            return 0xFFFD;
        }
        cp = (cp << 6) | (d & 0x3F);
    }
    *pp = (const char *)(p + 1 + need);
    return cp;
}

static Atlas *Pick(unsigned cp)
{
    if (cp >= 0xE000u && cp < 0xE200u && s_at[2].ok)
        return &s_at[2];
    if (cp >= 0x3040u && cp < 0x3100u && s_at[1].ok)
        return &s_at[1];
    if (s_at[0].ok)
        return &s_at[0];
    return 0;
}

static float Pen(const char *s, float x, float y, float px, bool dry,
                 float r, float g, float b)
{
    if (!s || !s_ok)
        return x;
    float pen = x;
    while (*s) {
        unsigned cp = Next(&s);
        Atlas *a = Pick(cp);
        if (!a)
            return pen;
        if (a != &s_at[0] &&
            (cp < (unsigned)a->first ||
             cp >= (unsigned)(a->first + a->count))) {
            a = &s_at[0];
            cp = 63u;
        }

        if (a == &s_at[0] && (cp < 32u || cp > 255u))
            cp = 63u;
        if (cp < (unsigned)a->first ||
            cp >= (unsigned)(a->first + a->count))
            continue;
        float scale = px / a->px;
        if (pen > 1920.0f - 16.0f)
            break;
        float before = pen;
        float xpos = pen;
        float ypos = y;
        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(a->g, FONT_ATLAS_W, FONT_ATLAS_H,
                           (int)(cp - (unsigned)a->first), &xpos, &ypos,
                           &q, 1);
        float x0 = before + (q.x0 - before) * scale;
        float x1 = before + (q.x1 - before) * scale;
        float y0 = y + (q.y0 - y) * scale;
        float y1 = y + (q.y1 - y) * scale;
        pen = before + (xpos - before) * scale;
        if (!dry && x1 > x0 && y1 > y0)
            Gfx::PushText(a->slot, x0, y0, x1, y1, q.s0, q.t0, q.s1, q.t1,
                          r, g, b);
    }
    return pen;
}

float Draw(const char *s, float x, float y, float px,
           float r, float g, float b)
{
    return Pen(s, x, y, px, false, r, g, b);
}

float Measure(const char *s, float px)
{
    return Pen(s, 0.0f, 0.0f, px, true, 0.0f, 0.0f, 0.0f);
}

void Centered(const char *s, float cx, float y, float px, float maxW,
              float r, float g, float b)
{
    float w = Measure(s, px);
    while (w > maxW && px > 14.0f) {
        px -= 2.0f;
        w = Measure(s, px);
    }
    Draw(s, cx - w * 0.5f, y, px, r, g, b);
}

} /* namespace Font */

