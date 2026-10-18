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

constexpr int kAvatarMax = 2;

namespace Avatars {

    namespace {

        /// @brief All mutable avatar cache state, single global.
        struct AvatarState {
            int avatarCount;
            int textureSlots[kAvatarMax] = { -1, -1 };
            bool slotReady[kAvatarMax];
            AccountUid userIds[kAvatarMax];
            s32 userTotal;
            bool hasUserIds;
        };

        static_assert(kAvatarMax == 2, "keep -1 slot init in sync");

        static AvatarState s_state = {};

    } // namespace

    /// @brief Free one avatar's GPU resources.
    /// @param avatarIndex Avatar slot.
    static void FreeOne(int avatarIndex)
    {
        if (s_state.slotReady[avatarIndex]) {
            s_state.slotReady[avatarIndex] = false;
        }

        Gfx::TexFree(s_state.textureSlots[avatarIndex]);
        s_state.textureSlots[avatarIndex] = -1;
    }

    /// @brief Upload RGBA pixels into an avatar slot.
    /// @param avatarIndex Avatar slot.
    /// @param pixelData Pixel data.
    /// @param imageWidth Width.
    /// @param imageHeight Height.
    /// @return True on success.
    static bool UploadOne(int avatarIndex, const uint8_t *pixelData, int imageWidth, int imageHeight)
    {
        if (!pixelData || imageWidth <= 0 || imageHeight <= 0 || imageWidth > 512 || imageHeight > 512) {
            return false;
        }

        FreeOne(avatarIndex);

        if (s_state.textureSlots[avatarIndex] < 0) {
            s_state.textureSlots[avatarIndex] = Gfx::TexAlloc();

            if (s_state.textureSlots[avatarIndex] < 0) {
                return false;
            }
        }

        if (!Gfx::TexUpload(s_state.textureSlots[avatarIndex], pixelData, imageWidth, imageHeight, Gfx::TexFormat::RGBA)) {
            return false;
        }

        s_state.slotReady[avatarIndex] = true;
        return true;
    }

    /// @brief Refresh cached avatar images from accounts.
    void Refresh()
    {
        AccountUid userIds[kAvatarMax] = {};
        int userTotal = qext_account_list(userIds, kAvatarMax);

        if (userTotal < 0) {
            return;
        }

        if (s_state.hasUserIds && userTotal == s_state.userTotal && memcmp(userIds, s_state.userIds, sizeof(userIds)) == 0) {
            return;
        }

        for (int avatarIndex = 0; avatarIndex < kAvatarMax; avatarIndex++) {
            FreeOne(avatarIndex);
        }

        s_state.avatarCount = 0;

        for (int avatarIndex = 0; avatarIndex < kAvatarMax && avatarIndex < userTotal; avatarIndex++) {
            if (!qext_account_uid_valid(userIds[avatarIndex])) {
                continue;
            }

            s_state.avatarCount++;
            int imageSize = qext_account_image_size(userIds[avatarIndex]);

            if (imageSize < 0) {
                continue;
            }

            uint8_t *jpegBuffer = (uint8_t *)malloc((unsigned)imageSize);
            uint8_t *pixelData = 0;

            if (jpegBuffer) {
                int bytesRead = qext_account_image(userIds[avatarIndex], jpegBuffer, (unsigned)imageSize);

                if (bytesRead > 2) {
                    int imageWidth = 0;
                    int imageHeight = 0;
                    pixelData = stbi_load_from_memory(jpegBuffer, bytesRead, &imageWidth, &imageHeight, NULL, 4);

                    if (pixelData && imageWidth > 0 && imageHeight > 0) {
                        UploadOne(avatarIndex, pixelData, imageWidth, imageHeight);
                    }

                    if (pixelData) {
                        stbi_image_free(pixelData);
                    }
                }

                free(jpegBuffer);
            }
        }

        memcpy(s_state.userIds, userIds, sizeof(userIds));
        s_state.userTotal = userTotal;
        s_state.hasUserIds = true;
    }

    /// @brief Number of cached avatars.
    /// @return Avatar count.
    int Count()
    {
        return s_state.avatarCount;
    }

    /// @brief Look up the icon slot for an avatar.
    /// @param avatarIndex Avatar index.
    /// @return HUD icon slot, or <= 0 when missing.
    int Slot(int avatarIndex)
    {
        if (avatarIndex < 0 || avatarIndex >= kAvatarMax || !s_state.slotReady[avatarIndex]) {
            return -1;
        }

        return s_state.textureSlots[avatarIndex];
    }

} // namespace Avatars
