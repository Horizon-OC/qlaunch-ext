/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */
#include "Icons.hpp"
#include "Gfx.hpp"
#include "Font.hpp"
#include "Theme.hpp"
#include "Qext.hpp"
#include "Packs.hpp"
#include <string.h>
#include <stdio.h>
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

namespace {

    /// @brief All mutable icon cache state, single global.
    struct IconsState {
        Icons::Entry titleEntries[kIconsMax];
        u64 sequenceCounter;
        bool hudReady[kHudCount];
        int hudTextures[kHudCount];
        bool slotsInitialized;
        char packDir[192];
    };

    static IconsState s_state = {};

/// @brief One-time slot init (-1 = owns nothing). Zero-init would read
/// as slot 0, which the manager may have handed to someone else.
static void EnsureSlots()
{
    if (s_state.slotsInitialized) {
        return;
    }
    s_state.slotsInitialized = true;
    for (int entryIndex = 0; entryIndex < kIconsMax; entryIndex++) {
        s_state.titleEntries[entryIndex].textureSlot = -1;
    }
    for (int hudIndex = 0; hudIndex < kHudCount; hudIndex++) {
        s_state.hudTextures[hudIndex] = -1;
    }
}

/* NOTE: dont make these static pointer tables otherwise there will be crashes due to relocation stuff */
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

    /// @brief Fetch, decode and upload one title icon into an entry.
    /// The entry must be freed first when it was in use.
    /// @param iconEntry Entry receiving the icon.
    /// @param titleId Title id to load.
    /// @return True when the entry holds a fresh icon.
    static bool LoadTitleIcon(Icons::Entry *iconEntry, u64 titleId)
    {
        int titleCount = qext_title_count();

        for (int titleIndex = 0; titleIndex < titleCount; titleIndex++) {
            if (qext_title_id(titleIndex) != titleId) {
                continue;
            }

            int iconSize = qext_title_icon_size(titleIndex);

            if (iconSize <= 0 || iconSize > 0x40000) {
                break;
            }

            uint8_t *jpegBuffer = (uint8_t *)malloc((unsigned)iconSize);

            if (!jpegBuffer) {
                break;
            }

            int bytesRead = qext_title_icon(titleIndex, jpegBuffer, (unsigned)iconSize);

            if (bytesRead > 2 && jpegBuffer[0] == 0xFF && jpegBuffer[1] == 0xD8) {
                int imageWidth = 0;
                int imageHeight = 0;
                uint8_t *pixelData = stbi_load_from_memory(
                    jpegBuffer, bytesRead, &imageWidth, &imageHeight, NULL, 4);

                if (pixelData && imageWidth > 0 && imageHeight > 0 && imageWidth <= 1024 && imageHeight <= 1024) {
                    if (iconEntry->textureSlot < 0) {
                        iconEntry->textureSlot = Gfx::TexAlloc();
                    }

                    if (iconEntry->textureSlot >= 0 && Icons::Upload(iconEntry, pixelData, imageWidth, imageHeight)) {
                        iconEntry->titleId = titleId;
                        iconEntry->isUsed = true;
                        free(jpegBuffer);

                        if (pixelData) {
                            stbi_image_free(pixelData);
                        }

                        return true;
                    }

                    Gfx::TexFree(iconEntry->textureSlot);
                    iconEntry->textureSlot = -1;
                }

                if (pixelData) {
                    stbi_image_free(pixelData);
                }
            }

            free(jpegBuffer);
            break;
        }

        return false;
    }

    /// @brief Decode PNG bytes with core art policy.
    /// @param pngData PNG bytes.
    /// @param pngSize Byte size.
    /// @param downscale True to shrink art larger than 256px.
    /// @param invertForDark True to invert dark art on dark themes.
    /// @param imageWidthOut Output width.
    /// @param imageHeightOut Output height.
    /// @return Pixels to release with stbi_image_free(), or null.
    static uint8_t *DecodePng(const uint8_t *pngData, unsigned pngSize, bool downscale,
                              bool invertForDark, int *imageWidthOut, int *imageHeightOut)
    {
        int imageWidth = 0;
        int imageHeight = 0;
        uint8_t *pixelData = nullptr;

        if (pngData && pngSize > 0 && pngSize <= 0x40000) {
            pixelData = stbi_load_from_memory(pngData, (int)pngSize, &imageWidth, &imageHeight, NULL, 4);
        }

        if (pixelData && downscale && (imageWidth > 256 || imageHeight > 256)) {
            pixelData = Downscale(pixelData, &imageWidth, &imageHeight);
        }

        if (pixelData && invertForDark && Theme::IsDark()) {
            unsigned long long luminanceSum = 0;
            unsigned opaqueCount = 0;

            for (int pixelIndex = 0; pixelIndex < imageWidth * imageHeight; pixelIndex++) {
                if (pixelData[pixelIndex * 4 + 3] > 8) {
                    luminanceSum += (unsigned)pixelData[pixelIndex * 4] * 30u +
                           (unsigned)pixelData[pixelIndex * 4 + 1] * 59u +
                           (unsigned)pixelData[pixelIndex * 4 + 2] * 11u;
                    opaqueCount++;
                }
            }

            if (opaqueCount > 0 && luminanceSum / opaqueCount < 128u * 100u) {
                for (int pixelIndex = 0; pixelIndex < imageWidth * imageHeight; pixelIndex++) {
                    pixelData[pixelIndex * 4] = (uint8_t)(255 - pixelData[pixelIndex * 4]);
                    pixelData[pixelIndex * 4 + 1] = (uint8_t)(255 - pixelData[pixelIndex * 4 + 1]);
                    pixelData[pixelIndex * 4 + 2] = (uint8_t)(255 - pixelData[pixelIndex * 4 + 2]);
                }
            }
        }

        if (!pixelData || imageWidth <= 0 || imageHeight <= 0 || imageHeight > 4096) {
            if (pixelData) {
                stbi_image_free(pixelData);
            }

            return nullptr;
        }

        *imageWidthOut = imageWidth;
        *imageHeightOut = imageHeight;
        return pixelData;
    }

    /// @brief Pack file stems per HUD id (matches menu/assets/hud names).
    const char *HudFileStem(int hudId)
    {
        static const char *kStems[kHudCount] = {
            "hud_user", "hud_btn_a", "hud_btn_plus", "hud_folder", "hud_vgccard",
            "top_sel_home", "top_sel_connect", "top_sel_eshop", "top_sel_album", "top_sel_settings",
            "top_des_home", "top_des_connect", "top_des_eshop", "top_des_album", "top_des_settings",
            "marker_sheet", "wifi_0", "wifi_1", "wifi_2", "wifi_3", "wifi_none", "airplane",
        };

        if (hudId < 0 || hudId >= kHudCount) {
            return nullptr;
        }

        return kStems[hudId];
    }

    /// @brief Decode pack PNG bytes into an existing HUD slot (core art policy).
    /// @param hudIndex HUD slot receiving the image.
    /// @param pngData PNG bytes.
    /// @param pngSize Byte size.
    /// @return True when the slot holds the pack image.
    bool OverrideHudSlot(int hudIndex, const uint8_t *pngData, unsigned pngSize)
    {
        bool downscaleArt = hudIndex != kHudMarkerStrip;
        int imageWidth = 0;
        int imageHeight = 0;
        uint8_t *pixelData = DecodePng(pngData, pngSize, downscaleArt, true,
                                       &imageWidth, &imageHeight);

        if (!pixelData) {
            return false;
        }

        if (imageWidth > 512 && hudIndex != kHudMarkerStrip) {
            stbi_image_free(pixelData);
            return false;
        }

        if (s_state.hudTextures[hudIndex] < 0) {
            s_state.hudTextures[hudIndex] = Gfx::TexAlloc();

            if (s_state.hudTextures[hudIndex] < 0) {
                stbi_image_free(pixelData);
                return false;
            }
        }

        if (!Gfx::TexUpload(s_state.hudTextures[hudIndex], pixelData, imageWidth, imageHeight,
                            Gfx::TexFormat::RGBA)) {
            stbi_image_free(pixelData);
            return false;
        }

        stbi_image_free(pixelData);
        s_state.hudReady[hudIndex] = true;
        return true;
    }

} // namespace

bool Icons::UploadPng(const uint8_t *pngData, unsigned pngSize, bool downscale, int *textureSlotOut)
{
    EnsureSlots();

    if (!textureSlotOut) {
        return false;
    }

    *textureSlotOut = -1;
    int imageWidth = 0;
    int imageHeight = 0;
    uint8_t *pixelData = DecodePng(pngData, pngSize, downscale, true, &imageWidth, &imageHeight);

    if (!pixelData) {
        return false;
    }

    int textureSlot = Gfx::TexAlloc();

    if (textureSlot < 0) {
        stbi_image_free(pixelData);
        return false;
    }

    if (!Gfx::TexUpload(textureSlot, pixelData, imageWidth, imageHeight, Gfx::TexFormat::RGBA)) {
        stbi_image_free(pixelData);
        Gfx::TexFree(textureSlot);
        return false;
    }

    stbi_image_free(pixelData);
    *textureSlotOut = textureSlot;
    return true;
}

bool Icons::InitHud(const HudDef *assetDefs, int assetCount)
{
    EnsureSlots();

    if (!assetDefs || assetCount <= 0) {
        return false;
    }

    if (assetCount > kHudCount) {
        assetCount = kHudCount;
    }

    bool anyLoaded = false;

    for (int hudIndex = 0; hudIndex < assetCount; hudIndex++) {
        if (s_state.hudReady[hudIndex]) {
            continue;
        }

        uint32_t pngSize = (assetDefs[hudIndex].size != nullptr) ? *assetDefs[hudIndex].size : 0;

        if (assetDefs[hudIndex].png == nullptr || pngSize == 0) {
            continue;
        }

        bool downscaleArt = hudIndex != kHudMarkerStrip;
        int imageWidth = 0;
        int imageHeight = 0;
        uint8_t *pixelData = DecodePng(assetDefs[hudIndex].png, pngSize, downscaleArt,
                                       !assetDefs[hudIndex].noInvert, &imageWidth, &imageHeight);

        if (!pixelData) {
            continue;
        }

        if (imageWidth > 512 && hudIndex != kHudMarkerStrip) {
            stbi_image_free(pixelData);
            continue;
        }

        if (s_state.hudTextures[hudIndex] < 0) {
            s_state.hudTextures[hudIndex] = Gfx::TexAlloc();

            if (s_state.hudTextures[hudIndex] < 0) {
                stbi_image_free(pixelData);
                continue;
            }
        }

        if (!Gfx::TexUpload(s_state.hudTextures[hudIndex], pixelData, imageWidth, imageHeight, Gfx::TexFormat::RGBA)) {
            stbi_image_free(pixelData);
            continue;
        }

        stbi_image_free(pixelData);
        s_state.hudReady[hudIndex] = true;
        anyLoaded = true;
    }

    return anyLoaded;
}

void Icons::ResetHud()
{
    EnsureSlots();

    for (int hudIndex = 0; hudIndex < kHudCount; hudIndex++) {
        s_state.hudReady[hudIndex] = false;
        Gfx::TexFree(s_state.hudTextures[hudIndex]);
        s_state.hudTextures[hudIndex] = -1;
    }
}

bool Icons::InitHudPack(const char *packDir)
{
    EnsureSlots();
    s_state.packDir[0] = 0;

    if (!packDir || !packDir[0]) {
        return false;
    }

    snprintf(s_state.packDir, sizeof(s_state.packDir), "%s", packDir);
    s_state.packDir[sizeof(s_state.packDir) - 1] = 0;
    return ReapplyPack();
}

bool Icons::ReapplyPack()
{
    EnsureSlots();

    if (!s_state.packDir[0]) {
        return false;
    }

    bool anyOverridden = false;

    for (int hudIndex = 0; hudIndex < kHudCount; hudIndex++) {
        const char *stem = HudFileStem(hudIndex);

        if (!stem) {
            continue;
        }

        char pngPath[256];
        snprintf(pngPath, sizeof(pngPath), "%s/hud/%s.png", s_state.packDir, stem);
        pngPath[sizeof(pngPath) - 1] = 0;

        unsigned pngSize = 0;
        unsigned char *pngData = Packs::ReadFile(pngPath, 0x40000, &pngSize);

        if (!pngData) {
            continue;
        }

        if (OverrideHudSlot(hudIndex, pngData, pngSize)) {
            anyOverridden = true;
        }

        free(pngData);
    }

    return anyOverridden;
}

int Icons::HudSlot(int hudId)
{
    EnsureSlots();

    if (hudId < 0 || hudId >= kHudCount || !s_state.hudReady[hudId]) {
        return -1;
    }

    return s_state.hudTextures[hudId];
}

bool Icons::Upload(Icons::Entry *iconEntry, const uint8_t *pixelData, int imageWidth, int imageHeight)
{
    if (!pixelData || imageWidth <= 0 || imageHeight <= 0 || imageWidth > 1024 || imageHeight > 1024) {
        return false;
    }

    EnsureSlots();

    if (iconEntry->textureSlot < 0) {
        iconEntry->textureSlot = Gfx::TexAlloc();

        if (iconEntry->textureSlot < 0) {
            return false;
        }
    }

    return Gfx::TexUpload(iconEntry->textureSlot, pixelData, imageWidth, imageHeight, Gfx::TexFormat::RGBA);
}

void Icons::Free(Icons::Entry *iconEntry)
{
    EnsureSlots();

    if (iconEntry->isUsed) {
        iconEntry->isUsed = false;
        iconEntry->titleId = 0;
    }

    Gfx::TexFree(iconEntry->textureSlot);
    iconEntry->textureSlot = -1;
}

void Icons::FreeAll()
{
    EnsureSlots();

    for (int entryIndex = 0; entryIndex < kIconsMax; entryIndex++) {
        Free(&s_state.titleEntries[entryIndex]);
    }

    for (int hudIndex = 0; hudIndex < kHudCount; hudIndex++) {
        s_state.hudReady[hudIndex] = false;
        Gfx::TexFree(s_state.hudTextures[hudIndex]);
        s_state.hudTextures[hudIndex] = -1;
    }
}

void Icons::Sync(const u64 *titleIds, int *outputSlots, int rowCount, int focusedRow)
{
    if (!titleIds || !outputSlots || rowCount <= 0) {
        return;
    }

    if (focusedRow < 0) {
        focusedRow = 0;
    }

    if (focusedRow >= rowCount) {
        focusedRow = rowCount - 1;
    }

    for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) {
        outputSlots[rowIndex] = -1;
    }

    /* Outward from the focus, so the visible window owns the fixed
       budget. Entries stamped this pass are never evicted by it. */
    u64 passStartSequence = s_state.sequenceCounter;
    int loadCount = 0;

    for (int distance = 0; distance < rowCount; distance++) {
        for (int sideIndex = 0; sideIndex < 2; sideIndex++) {
            if (sideIndex == 1 && distance == 0) {
                continue;
            }

            int rowIndex = (sideIndex == 0) ? focusedRow + distance : focusedRow - distance;

            if (rowIndex < 0 || rowIndex >= rowCount) {
                continue;
            }

            u64 titleId = titleIds[rowIndex];

            if (!titleId) {
                continue;
            }

            Entry *cacheHit = nullptr;

            for (int entryIndex = 0; entryIndex < kIconsMax; entryIndex++) {
                if (s_state.titleEntries[entryIndex].isUsed && s_state.titleEntries[entryIndex].titleId == titleId) {
                    cacheHit = &s_state.titleEntries[entryIndex];
                    break;
                }
            }

            if (cacheHit) {
                cacheHit->sequence = ++s_state.sequenceCounter;
                outputSlots[rowIndex] = cacheHit->textureSlot;
                continue;
            }

            /* At most a full turnover per pass; the rest keeps
               fallback tiles until scrolling re-runs the pass. */
            if (loadCount >= kIconsMax) {
                continue;
            }

            Entry *victimEntry = nullptr;

            for (int entryIndex = 0; entryIndex < kIconsMax; entryIndex++) {
                if (!s_state.titleEntries[entryIndex].isUsed) {
                    victimEntry = &s_state.titleEntries[entryIndex];
                    break;
                }
            }

            if (!victimEntry) {
                for (int entryIndex = 0; entryIndex < kIconsMax; entryIndex++) {
                    if (!s_state.titleEntries[entryIndex].isUsed || s_state.titleEntries[entryIndex].sequence > passStartSequence) {
                        continue;
                    }

                    if (!victimEntry || s_state.titleEntries[entryIndex].sequence < victimEntry->sequence) {
                        victimEntry = &s_state.titleEntries[entryIndex];
                    }
                }
            }

            if (!victimEntry) {
                continue;
            }

            Free(victimEntry);

            if (LoadTitleIcon(victimEntry, titleId)) {
                victimEntry->sequence = ++s_state.sequenceCounter;
                outputSlots[rowIndex] = victimEntry->textureSlot;
                loadCount++;
            }
        }
    }
}

