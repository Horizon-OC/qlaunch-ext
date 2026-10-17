/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Avatars.hpp"
#include "Gfx.hpp"
#include "Qext.hpp"
#include <switch.h>
#include <string.h>
#include <stdlib.h>

#include "../third_party/stb_image.h"

constexpr int AV_MAX = 2;

namespace Avatars {

static int s_count;
static int s_Texture[AV_MAX] = { -1, -1 };
static_assert(AV_MAX == 2, "keep -1 init in sync");
static bool s_ok[AV_MAX];

    /// @brief Free one avatar's GPU resources.
    /// @param i Avatar slot.
    static void FreeOne(int i) {
        if (s_ok[i]) {
            s_ok[i] = false;
        }
        Gfx::TexFree(s_Texture[i]);
        s_Texture[i] = -1;
    }

    /// @brief Upload RGBA pixels into an avatar slot.
    /// @param i Avatar slot.
    /// @param rgba Pixel data.
    /// @param w Width.
    /// @param h Height.
    /// @return True on success.
    static bool UploadOne(int i, const uint8_t *rgba, int w, int h)
{
    if (!rgba || w <= 0 || h <= 0 || w > 512 || h > 512)
        return false;
    FreeOne(i);
    if (s_Texture[i] < 0) {
        s_Texture[i] = Gfx::TexAlloc();
        if (s_Texture[i] < 0)
            return false;
    }
    if (!Gfx::TexUpload(s_Texture[i], rgba, w, h, Gfx::TexFormat::RGBA))
        return false;
    s_ok[i] = true;
    return true;
}

static AccountUid s_uids[AV_MAX];
static s32 s_uidTotal = -1;
static bool s_haveUids = false;

    /// @brief Refresh cached avatar images from accounts.
    void Refresh()
{
    AccountUid uids[AV_MAX] = {};
    int total = qext_account_list(uids, AV_MAX);
    if (total < 0)
        return;

    if (s_haveUids && total == s_uidTotal && memcmp(uids, s_uids, sizeof(uids)) == 0)
        return;
    for (int i = 0; i < AV_MAX; i++)
        FreeOne(i);

    s_count = 0;
    for (int i = 0; i < AV_MAX && i < total; i++) {
        if (!qext_account_uid_valid(uids[i]))
            continue;
        s_count++;
        int size = qext_account_image_size(uids[i]);
        if (size < 0)
            continue;
        uint8_t *jpg = (uint8_t *)malloc((unsigned)size);
        uint8_t *px = 0;
        if (jpg) {
            int done = qext_account_image(uids[i], jpg, (unsigned)size);
            if (done > 2) {
                int w = 0, h = 0;
                px = stbi_load_from_memory(jpg, done, &w, &h, NULL, 4);
                if (px && w > 0 && h > 0)
                    UploadOne(i, px, w, h);
                if (px)
                    stbi_image_free(px);
            }
            free(jpg);
        }
    }
    memcpy(s_uids, uids, sizeof(uids));
    s_uidTotal = total;
    s_haveUids = true;
}

    /// @brief Number of cached avatars.
    /// @return Avatar count.
    int Count()
{
    return s_count;
}

    /// @brief Look up the icon slot for an avatar.
    /// @param i Avatar index.
    /// @return HUD icon slot, or <= 0 when missing.
    int Slot(int i)
{
    if (i < 0 || i >= AV_MAX || !s_ok[i])
        return -1;
    return s_Texture[i];
}

} // namespace Avatars