/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Layout.hpp"
#include "../core/App.hpp"
#include "../core/Sfx.hpp"
#include "../core/Qext.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "../third_party/stb_image.h"

namespace WAlbum {

static bool s_open = false;
static bool s_full = false;
static int s_sel = 0;
static int s_top = 0;
static int s_count = 0;

enum { COLS = 4, ROWS = 2, VIS = COLS * ROWS };
enum { THUMB_SLOTS = 8, FULL_SLOT = 8 };

struct Slot {
    bool used;
    int index;
    int tex; /* dynamic TexAlloc slot, <=2 = none */
    DkMemBlock mem;
    DkImage img;
};

static Slot s_thumb[THUMB_SLOTS];
static Slot s_fullImg;
static uint8_t *s_jpg = 0;
static uint8_t *s_big = 0;
static int s_thumbErr[THUMB_SLOTS];
static int s_fullErr = -1;
static bool s_fresh = false;

static void FreeSlot(Slot *s)
{
    if (s->used) {
        Gfx::WaitIdle();
        dkMemBlockDestroy(s->mem);
        s->used = false;
        s->index = -1;
        s->mem = 0;
    }
    Gfx::TexFree(s->tex);
    s->tex = -1;
}

static bool UploadSlot(Slot *s, const uint8_t *rgba, int w, int h)
{
    if (!rgba || w <= 0 || h <= 0 || w > 1920 || h > 1080)
        return false;
    FreeSlot(s);
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
    if (s->tex <= 2) {
        s->tex = Gfx::TexAlloc();
        if (s->tex < 0) {
            dkMemBlockDestroy(mem);
            return false;
        }
    }
    dkImageInitialize(&s->img, &lay, mem, 0);
    s->mem = mem;
    s->used = true;
    DkImageView view;
    dkImageViewDefaults(&view, &s->img);
    Gfx::WriteImageDesc(s->tex, &view);
    return true;
}

static bool EnsureThumb(int pos, int albumIndex)
{
    Slot *s = &s_thumb[pos];
    if (s->used && s->index == albumIndex)
        return true;
    if (!s_jpg) {
        s_jpg = (uint8_t *)malloc(0x40000);
        if (!s_jpg)
            return false;
    }
    int got = qext_album_thumb(albumIndex, s_jpg, 0x40000);
    if (got <= 2) {
        if (s_thumbErr[pos] != albumIndex) {
            s_thumbErr[pos] = albumIndex;
            logging::LogLine("[album] thumb %d: load got=%d", albumIndex, got);
        }
        return false;
    }
    int w = 0, h = 0;
    uint8_t *px = stbi_load_from_memory(s_jpg, got, &w, &h, NULL, 4);
    if (!px || w <= 0 || h <= 0) {
        if (s_thumbErr[pos] != albumIndex) {
            s_thumbErr[pos] = albumIndex;
            logging::LogLine("[album] thumb %d: jpeg decode failed (%d bytes)", albumIndex, got);
        }
        if (px)
            stbi_image_free(px);
        return false;
    }
    s_thumbErr[pos] = -1;
    bool ok = UploadSlot(s, px, w, h);
    stbi_image_free(px);
    if (ok)
        s->index = albumIndex;
    return ok;
}

static bool EnsureFull(int albumIndex)
{
    if (s_fullImg.used && s_fullImg.index == albumIndex)
        return true;
    int need = qext_album_image_size(albumIndex);
    if (need <= 0 || need > 0x800000) {
        if (s_fullErr != albumIndex) {
            s_fullErr = albumIndex;
            logging::LogLine("[album] full %d: size=%d", albumIndex, need);
        }
        return false;
    }
    if (!s_big) {
        s_big = (uint8_t *)malloc(0x800000);
        if (!s_big)
            return false;
    }
    if ((unsigned)need > 0x800000)
        return false;
    int got = qext_album_image(albumIndex, s_big, (unsigned)need);
    if (got <= 2) {
        if (s_fullErr != albumIndex) {
            s_fullErr = albumIndex;
            logging::LogLine("[album] full %d: load got=%d need=%d", albumIndex, got, need);
        }
        return false;
    }
    int w = 0, h = 0;
    uint8_t *px = stbi_load_from_memory(s_big, got, &w, &h, NULL, 4);
    if (!px || w <= 0 || h <= 0) {
        if (s_fullErr != albumIndex) {
            s_fullErr = albumIndex;
            logging::LogLine("[album] full %d: jpeg decode failed (%d bytes)", albumIndex, got);
        }
        if (px)
            stbi_image_free(px);
        return false;
    }
    s_fullErr = -1;
    bool ok = UploadSlot(&s_fullImg, px, w, h);
    stbi_image_free(px);
    if (ok)
        s_fullImg.index = albumIndex;
    return ok;
}

static void ClearThumbs()
{
    for (int i = 0; i < THUMB_SLOTS; i++) {
        FreeSlot(&s_thumb[i]);
        s_thumbErr[i] = -1;
    }
}

static void ClampSel()
{
    if (s_count <= 0) {
        s_sel = 0;
        s_top = 0;
        return;
    }
    if (s_sel < 0)
        s_sel = 0;
    if (s_sel >= s_count)
        s_sel = s_count - 1;
    s_top -= s_top % COLS;
    if (s_top < 0)
        s_top = 0;
    while (s_sel < s_top)
        s_top -= COLS;
    while (s_sel >= s_top + VIS)
        s_top += COLS;
    if (s_top < 0)
        s_top = 0;
}

bool IsOpen()
{
    return s_open;
}

bool IsFull()
{
    return s_open && s_full;
}

void Open()
{
    s_count = 0;
    for (int r = 0; r < 3 && s_count <= 0; r++)
        s_count = qext_album_refresh();
    logging::LogLine("[album] opened: %d images", s_count);
    if (s_count < 0)
        s_count = 0;
    s_sel = 0;
    s_top = 0;
    s_full = false;
    s_fresh = true;
    ClearThumbs();
    FreeSlot(&s_fullImg);
    s_open = true;
}

void Close()
{
    s_open = false;
    s_full = false;
    ClearThumbs();
    FreeSlot(&s_fullImg);
}

void OnRefresh()
{
    if (!s_open)
        return;
    ClearThumbs();
    FreeSlot(&s_fullImg);
    s_full = false;
}

static void DrawGrid()
{
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);

    /* Opaque viewer: the home strip stays hidden underneath. */
    float bgR, bgG, bgB;
    Theme::Color("bg", &bgR, &bgG, &bgB);
    Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, bgR, bgG, bgB, 1.0f);

    (void)inkR;
    (void)inkG;
    (void)inkB;

    if (s_count <= 0) {
        Font::Centered("No screenshots yet", 960.0f, 500.0f, 40.0f, 1200.0f,
                       dimR, dimG, dimB);
        Font::Centered("Capture with the Share button", 960.0f, 560.0f, 32.0f,
                       1200.0f, dimR, dimG, dimB);
    } else {
        float tw = 400.0f, th = 225.0f, gap = 32.0f;
        float totalW = COLS * tw + (COLS - 1) * gap;
        float x0 = (1920.0f - totalW) * 0.5f;
        float y0 = 240.0f;
        int base = s_top;
        for (int p = 0; p < VIS; p++) {
            int idx = base + p;
            if (idx >= s_count)
                break;
            int cx = p % COLS, cy = p / COLS;
            float x = x0 + cx * (tw + gap);
            float y = y0 + cy * (th + 56.0f + gap);
            bool sel = (idx == s_sel);
            float fr, fg, fb;
            Theme::Color("panel", &fr, &fg, &fb);
            Gfx::PushPanel(x - 4.0f, y - 4.0f, tw + 8.0f,
                           th + 8.0f, 10.0f, fr, fg, fb, 1.0f);
            if (sel)
                Gfx::PushSelectRing(x - 10.0f, y - 10.0f, tw + 20.0f, th + 20.0f, 18.0f, 8.0f);
            if (EnsureThumb(p, idx)) {
                Gfx::PushIcon(x, y, tw, th, 8.0f, 1.0f, 1.0f, 1.0f, 0.0f, s_thumb[p].tex);
            } else {
                Gfx::PushPanel(x, y, tw, th, 8.0f, 0.25f, 0.26f, 0.30f, 1.0f);
            }
            char lb[32];
            if (qext_album_label(idx, lb, sizeof(lb)) > 0)
                Font::Draw(lb, x, y + th + 52.0f, 24.0f, dimR, dimG, dimB);
        }
    }
}

static void DrawFull()
{
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);

    if (s_sel < 0 || s_sel >= s_count) {
        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        Font::Centered("No image", 960.0f, 540.0f, 40.0f, 800.0f, inkR, inkG, inkB);
    } else if (EnsureFull(s_sel)) {
        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        Gfx::PushIcon(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, s_fullImg.tex, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);
    } else {
        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        Font::Centered("Could not load image", 960.0f, 540.0f, 40.0f, 800.0f,
                       1.0f, 1.0f, 1.0f);
    }
}

void Draw()
{
    if (!s_open)
        return;
    if (s_full)
        DrawFull();
    else
        DrawGrid();
}

bool Input(u64 down, u64 held)
{
    (void)held;
    if (!s_open)
        return false;

    /* Remove any a presses if they opened the viewer */
    if (s_fresh) {
        s_fresh = false;
        return true;
    }

    if (s_full) {
        if (down & HidNpadButton_B) {
            s_full = false;
            FreeSlot(&s_fullImg);
            Sfx::Play(Sfx::Back);
            return true;
        }
        if (down & HidNpadButton_AnyLeft) {
            if (s_sel > 0) {
                s_sel--;
                ClampSel();
                Sfx::Play(Sfx::Hover);
            }
            return true;
        }
        if (down & HidNpadButton_AnyRight) {
            if (s_sel < s_count - 1) {
                s_sel++;
                ClampSel();
                Sfx::Play(Sfx::Hover);
            }
            return true;
        }
        if (down & HidNpadButton_Plus) {
            s_count = qext_album_refresh();
            if (s_count < 0)
                s_count = 0;
            ClampSel();
            FreeSlot(&s_fullImg);
            return true;
        }
        if (down & HidNpadButton_A) {
            s_full = false;
            FreeSlot(&s_fullImg);
            Sfx::Play(Sfx::Back);
            return true;
        }
        return true;
    }

    if (down & HidNpadButton_B) {
        Sfx::Play(Sfx::Back);
        Close();
        App::SwitchMenu(MENU_HOME);
        return true;
    }

    if (down & HidNpadButton_A) {
        if (s_count > 0) {
            s_full = true;
            Sfx::Play(Sfx::Click);
        }
        return true;
    }

    if (down & HidNpadButton_Plus) {
        s_count = qext_album_refresh();
        if (s_count < 0)
            s_count = 0;
        ClampSel();
        ClearThumbs();
        return true;
    }

    if (down & HidNpadButton_AnyLeft) {
        if (s_sel > 0) {
            s_sel--;
            ClampSel();
            Sfx::Play(Sfx::Hover);
        }
        return true;
    }

    if (down & HidNpadButton_AnyRight) {
        if (s_sel < s_count - 1) {
            s_sel++;
            ClampSel();
            Sfx::Play(Sfx::Hover);
        }
        return true;
    }

    if (down & HidNpadButton_AnyUp) {
        if (s_sel - COLS >= 0) {
            s_sel -= COLS;
            ClampSel();
            Sfx::Play(Sfx::Hover);
        }
        return true;
    }

    if (down & HidNpadButton_AnyDown) {
        if (s_sel + COLS < s_count) {
            s_sel += COLS;
            ClampSel();
            Sfx::Play(Sfx::Hover);
        }
        return true;
    }
    
    return true;
}

} // namespace WAlbum
