/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Avatars.hpp"
#include "Gfx.hpp"
#include <switch.h>
#include <string.h>
#include <stdlib.h>

#include "../third_party/stb_image.h"

enum { AV_MAX = 2 };

namespace Avatars {

static int s_count;
static int s_tex[AV_MAX] = { 0 };
static DkMemBlock s_mem[AV_MAX];
static DkImage s_img[AV_MAX];
static bool s_ok[AV_MAX];

static void FreeOne(int i)
{
    if (s_ok[i]) {
        Gfx::WaitIdle();
        dkMemBlockDestroy(s_mem[i]);
        s_ok[i] = false;
        s_mem[i] = 0;
    }
    Gfx::TexFree(s_tex[i]);
    s_tex[i] = -1;
}

static bool UploadOne(int i, const uint8_t *rgba, int w, int h)
{
    if (!rgba || w <= 0 || h <= 0 || w > 512 || h > 512)
        return false;
    FreeOne(i);
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
    if (s_tex[i] <= 2) {
        s_tex[i] = Gfx::TexAlloc();
        if (s_tex[i] < 0) {
            dkMemBlockDestroy(mem);
            return false;
        }
    }
/* Pitch-linear rows may be padded: copy row by row unless packed. */
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
    dkImageInitialize(&s_img[i], &lay, mem, 0);
    s_mem[i] = mem;
    DkImageView view;
    dkImageViewDefaults(&view, &s_img[i]);
    Gfx::WriteImageDesc(s_tex[i], &view);
    s_ok[i] = true;
    return true;
}

static AccountUid s_uids[AV_MAX];
static s32 s_uidTotal = -1;
static bool s_haveUids = false;

void Refresh()
{
    AccountUid uids[AV_MAX] = {};
    s32 total = 0;
    if (R_FAILED(accountListAllUsers(uids, AV_MAX, &total)))
        return;

    if (s_haveUids && total == s_uidTotal && memcmp(uids, s_uids, sizeof(uids)) == 0)
        return;
    for (int i = 0; i < AV_MAX; i++)
        FreeOne(i);

    s_count = 0;
    for (int i = 0; i < AV_MAX && i < total; i++) {
        if (!accountUidIsValid(&uids[i]))
            continue;
        s_count++;
        AccountProfile prof{};
        if (R_FAILED(accountGetProfile(&prof, uids[i])))
            continue;
        u32 size = 0;
        uint8_t *jpg = 0;
        uint8_t *px = 0;
        if (R_SUCCEEDED(accountProfileGetImageSize(&prof, &size)) &&
            size > 0 && size <= 0x40000) {
            jpg = (uint8_t *)malloc(size);
            u32 done = 0;
            if (jpg &&
                R_SUCCEEDED(accountProfileLoadImage(&prof, jpg, size,
                                                    &done)) &&
                done > 2) {
                int w = 0, h = 0;
                px = stbi_load_from_memory(jpg, (int)done, &w, &h, NULL,
                                           4);
                if (px && w > 0 && h > 0)
                    UploadOne(i, px, w, h);
                if (px)
                    stbi_image_free(px);
            }
            if (jpg)
                free(jpg);
        }
        accountProfileClose(&prof);
    }
    memcpy(s_uids, uids, sizeof(uids));
    s_uidTotal = total;
    s_haveUids = true;
}

int Count()
{
    return s_count;
}

int Slot(int i)
{
    if (i < 0 || i >= AV_MAX || !s_ok[i])
        return -1;
    return s_tex[i];
}

} // namespace Avatars