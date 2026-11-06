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
#include <unordered_map>
#include <vector>

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

    struct TitleEntry {
        int textureSlot = -1;
        u64 usedTick = 0;
        bool isMiss = false;
    };

    struct IconsState {
        std::unordered_map<u64, TitleEntry> titleCache;
        std::vector<int> titleSlotPool;
        u64 titleClock = 0;
        int titleLive = 0;
        bool slotsInitialized = false;
        bool hudReady[kHudCount];
        bool hudAttempted[kHudCount];
        int hudTextures[kHudCount];
        char packDir[192];
    };

    static IconsState s_state = {};

    static void EnsureSlots()
    {
        if (s_state.slotsInitialized) {
            return;
        }
        s_state.slotsInitialized = true;
        for (int hudIndex = 0; hudIndex < kHudCount; hudIndex++) {
            s_state.hudTextures[hudIndex] = -1;
            s_state.hudAttempted[hudIndex] = false;
            s_state.hudReady[hudIndex] = false;
        }
        s_state.packDir[0] = 0;
    }

    static uint8_t *Downscale(uint8_t *px, int *w, int *h)
    {
        int sw = *w, sh = *h;
        int dw = sw, dh = sh;
        while (dw > kIconScaleMax || dh > kIconScaleMax) {
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
            if (y1 <= y0) y1 = y0 + 1;
            for (int x = 0; x < dw; x++) {
                int x0 = x * sw / dw;
                int x1 = (x + 1) * sw / dw;
                if (x1 <= x0) x1 = x0 + 1;
                unsigned sr = 0, sg = 0, sb = 0, sa = 0;
                unsigned cnt = 0;
                for (int sy = y0; sy < y1; sy++) {
                    for (int sx = x0; sx < x1; sx++) {
                        uint8_t *p = px + ((size_t)sy * (size_t)sw + (size_t)sx) * 4;
                        sr += p[0]; sg += p[1]; sb += p[2]; sa += p[3];
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

    static int AcquireTitleSlot()
    {
        if (!s_state.titleSlotPool.empty()) {
            int slot = s_state.titleSlotPool.back();
            s_state.titleSlotPool.pop_back();
            return slot;
        }
        int slot = Gfx::TexAlloc();
        if (slot < 0) return -1;
        return slot;
    }

    static void ReleaseTitleSlot(int slot)
    {
        if (slot < 0) return;
        s_state.titleSlotPool.push_back(slot);
    }

    static void EvictOldestTitle()
    {
        auto oldest = s_state.titleCache.end();
        for (auto it = s_state.titleCache.begin(); it != s_state.titleCache.end(); ++it) {
            if (it->second.isMiss) continue;
            if (oldest == s_state.titleCache.end() || it->second.usedTick < oldest->second.usedTick)
                oldest = it;
        }
        if (oldest != s_state.titleCache.end()) {
            if (oldest->second.textureSlot >= 0)
                ReleaseTitleSlot(oldest->second.textureSlot);
            s_state.titleCache.erase(oldest);
            if (s_state.titleLive > 0) s_state.titleLive--;
        }
    }

    static bool LoadTitleIconCached(u64 titleId, int *slotOut)
    {
        int titleCount = qext_title_count();
        for (int titleIndex = 0; titleIndex < titleCount; titleIndex++) {
            if (qext_title_id(titleIndex) != titleId) continue;
            int iconSize = qext_title_icon_size(titleIndex);
            if (iconSize <= 0 || iconSize > 0x40000) break;
            uint8_t *jpegBuffer = (uint8_t *)malloc((unsigned)iconSize);
            if (!jpegBuffer) break;
            int bytesRead = qext_title_icon(titleIndex, jpegBuffer, (unsigned)iconSize);
            if (bytesRead > 2 && jpegBuffer[0] == 0xFF && jpegBuffer[1] == 0xD8) {
                int imageWidth = 0, imageHeight = 0;
                uint8_t *pixelData = stbi_load_from_memory(jpegBuffer, bytesRead, &imageWidth, &imageHeight, NULL, 4);
                if (pixelData && imageWidth > 0 && imageHeight > 0 && imageWidth <= 1024 && imageHeight <= 1024) {
                    int texSlot = AcquireTitleSlot();
                    if (texSlot >= 0) {
                        if (Gfx::TexUpload(texSlot, pixelData, imageWidth, imageHeight, Gfx::TexFormat::RGBA)) {
                            free(jpegBuffer);
                            stbi_image_free(pixelData);
                            *slotOut = texSlot;
                            return true;
                        }
                        ReleaseTitleSlot(texSlot);
                    }
                    stbi_image_free(pixelData);
                } else if (pixelData) stbi_image_free(pixelData);
            }
            free(jpegBuffer);
            break;
        }
        return false;
    }

    static uint8_t *DecodePng(const uint8_t *pngData, unsigned pngSize, bool downscale, bool invertForDark, int *imageWidthOut, int *imageHeightOut)
    {
        int imageWidth = 0, imageHeight = 0;
        uint8_t *pixelData = nullptr;
        if (pngData && pngSize > 0 && pngSize <= 0x40000)
            pixelData = stbi_load_from_memory(pngData, (int)pngSize, &imageWidth, &imageHeight, NULL, 4);
        if (pixelData && downscale && (imageWidth > 256 || imageHeight > 256))
            pixelData = Downscale(pixelData, &imageWidth, &imageHeight);
        if (pixelData && invertForDark && Theme::IsDark()) {
            unsigned long long luminanceSum = 0;
            unsigned opaqueCount = 0;
            for (int i = 0; i < imageWidth * imageHeight; i++) {
                if (pixelData[i * 4 + 3] > 8) {
                    luminanceSum += (unsigned)pixelData[i * 4] * 30u + (unsigned)pixelData[i * 4 + 1] * 59u + (unsigned)pixelData[i * 4 + 2] * 11u;
                    opaqueCount++;
                }
            }
            if (opaqueCount > 0 && luminanceSum / opaqueCount < 128u * 100u) {
                for (int i = 0; i < imageWidth * imageHeight; i++) {
                    pixelData[i * 4] = 255 - pixelData[i * 4];
                    pixelData[i * 4 + 1] = 255 - pixelData[i * 4 + 1];
                    pixelData[i * 4 + 2] = 255 - pixelData[i * 4 + 2];
                }
            }
        }
        if (!pixelData || imageWidth <= 0 || imageHeight <= 0 || imageHeight > 4096) {
            if (pixelData) stbi_image_free(pixelData);
            return nullptr;
        }
        *imageWidthOut = imageWidth;
        *imageHeightOut = imageHeight;
        return pixelData;
    }

    static const char *HudFileStem(int hudId)
    {
        static const char *kStems[kHudCount] = {
            "hud_user", "hud_btn_a", "hud_btn_plus", "hud_folder",
            "top_sel_home", "top_sel_connect", "top_sel_eshop", "top_sel_album", "top_sel_settings",
            "top_des_home", "top_des_connect", "top_des_eshop", "top_des_album", "top_des_settings",
            "marker_sheet", "wifi_0", "wifi_1", "wifi_2", "wifi_3", "wifi_none", "airplane",
        };
        if (hudId < 0 || hudId >= kHudCount) return nullptr;
        return kStems[hudId];
    }

    static bool OverrideHudSlot(int hudIndex, const uint8_t *pngData, unsigned pngSize)
    {
        bool downscaleArt = hudIndex != kHudMarkerStrip;
        int imageWidth = 0, imageHeight = 0;
        uint8_t *pixelData = DecodePng(pngData, pngSize, downscaleArt, true, &imageWidth, &imageHeight);
        if (!pixelData) return false;
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
        if (!Gfx::TexUpload(s_state.hudTextures[hudIndex], pixelData, imageWidth, imageHeight, Gfx::TexFormat::RGBA)) {
            stbi_image_free(pixelData);
            Gfx::TexFree(s_state.hudTextures[hudIndex]);
            s_state.hudTextures[hudIndex] = -1;
            return false;
        }
        stbi_image_free(pixelData);
        s_state.hudReady[hudIndex] = true;
        return true;
    }

    static bool LoadHudSlot(int hudIndex)
    {
        EnsureSlots();
        if (hudIndex < 0 || hudIndex >= kHudCount) return false;
        if (s_state.hudReady[hudIndex]) return true;
        if (s_state.hudAttempted[hudIndex] || !s_state.packDir[0]) return s_state.hudReady[hudIndex];
        s_state.hudAttempted[hudIndex] = true;
        const char *stem = HudFileStem(hudIndex);
        if (!stem) return false;
        char pngPath[256];
        snprintf(pngPath, sizeof(pngPath), "%s/assets/hud/%s.png", s_state.packDir, stem);
        pngPath[sizeof(pngPath) - 1] = 0;
        unsigned pngSize = 0;
        unsigned char *pngData = (unsigned char *)Packs::ReadFile(pngPath, 0x40000, &pngSize);
        if (!pngData) return false;
        bool ok = OverrideHudSlot(hudIndex, pngData, pngSize);
        free(pngData);
        return ok;
    }

} // namespace

void Icons::Sync(const u64 *titleIds, int *outputSlots, int rowCount, int focusedRow, int maxLoadsPerPass)
{
    if (!titleIds || !outputSlots || rowCount <= 0) return;
    EnsureSlots();
    if (maxLoadsPerPass < 0) maxLoadsPerPass = 9999;
    if (focusedRow < 0) focusedRow = 0;
    if (focusedRow >= rowCount) focusedRow = rowCount - 1;
    for (int rowIndex = 0; rowIndex < rowCount; rowIndex++) outputSlots[rowIndex] = -1;

    int loadCount = 0;
    for (int distance = 0; distance < rowCount; distance++) {
        for (int sideIndex = 0; sideIndex < 2; sideIndex++) {
            if (sideIndex == 1 && distance == 0) continue;
            int rowIndex = (sideIndex == 0) ? focusedRow + distance : focusedRow - distance;
            if (rowIndex < 0 || rowIndex >= rowCount) continue;
            u64 titleId = titleIds[rowIndex];
            if (!titleId) continue;
            auto it = s_state.titleCache.find(titleId);
            if (it != s_state.titleCache.end()) {
                if (it->second.isMiss) {
                    it->second.usedTick = ++s_state.titleClock;
                    continue;
                }
                it->second.usedTick = ++s_state.titleClock;
                outputSlots[rowIndex] = it->second.textureSlot;
                continue;
            }
            if (loadCount >= maxLoadsPerPass) continue;
            if (s_state.titleLive >= kIconCacheCap) EvictOldestTitle();
            int slotOut = -1;
            if (LoadTitleIconCached(titleId, &slotOut)) {
                s_state.titleCache[titleId] = TitleEntry{slotOut, ++s_state.titleClock, false};
                s_state.titleLive++;
                outputSlots[rowIndex] = slotOut;
                loadCount++;
            } else {
                s_state.titleCache[titleId] = TitleEntry{-1, ++s_state.titleClock, true};
                loadCount++;
            }
        }
    }
}

void Icons::FreeAll()
{
    EnsureSlots();
    for (auto &kv : s_state.titleCache) {
        if (kv.second.textureSlot >= 0) {
            Gfx::TexFree(kv.second.textureSlot);
        }
    }
    s_state.titleCache.clear();
    s_state.titleCache.rehash(0);
    for (int sslot : s_state.titleSlotPool) {
        Gfx::TexFree(sslot);
    }
    s_state.titleSlotPool.clear();
    s_state.titleSlotPool.shrink_to_fit();
    s_state.titleLive = 0;
    s_state.titleClock = 0;
    for (int hudIndex = 0; hudIndex < kHudCount; hudIndex++) {
        s_state.hudReady[hudIndex] = false;
        s_state.hudAttempted[hudIndex] = false;
        Gfx::TexFree(s_state.hudTextures[hudIndex]);
        s_state.hudTextures[hudIndex] = -1;
    }
}

void Icons::ResetHud()
{
    EnsureSlots();
    for (int hudIndex = 0; hudIndex < kHudCount; hudIndex++) {
        s_state.hudReady[hudIndex] = false;
        s_state.hudAttempted[hudIndex] = false;
        Gfx::TexFree(s_state.hudTextures[hudIndex]);
        s_state.hudTextures[hudIndex] = -1;
    }
}

bool Icons::InitHudPack(const char *packDir)
{
    EnsureSlots();
    s_state.packDir[0] = 0;
    for (int hudIndex = 0; hudIndex < kHudCount; hudIndex++) s_state.hudAttempted[hudIndex] = false;
    if (!packDir || !packDir[0]) return false;
    snprintf(s_state.packDir, sizeof(s_state.packDir), "%s", packDir);
    s_state.packDir[sizeof(s_state.packDir) - 1] = 0;
    return true;
}

bool Icons::ReapplyPack()
{
    EnsureSlots();
    if (!s_state.packDir[0]) return false;
    bool anyOverridden = false;
    for (int hudIndex = 0; hudIndex < kHudCount; hudIndex++) {
        s_state.hudAttempted[hudIndex] = false;
        s_state.hudReady[hudIndex] = false;
        Gfx::TexFree(s_state.hudTextures[hudIndex]);
        s_state.hudTextures[hudIndex] = -1;
        if (LoadHudSlot(hudIndex)) anyOverridden = true;
    }
    return anyOverridden;
}

int Icons::HudSlot(int hudId)
{
    EnsureSlots();
    if (hudId < 0 || hudId >= kHudCount) return -1;
    if (!s_state.hudReady[hudId]) LoadHudSlot(hudId);
    if (!s_state.hudReady[hudId]) return -1;
    return s_state.hudTextures[hudId];
}

bool Icons::Upload(Entry *iconEntry, const uint8_t *pixelData, int imageWidth, int imageHeight)
{
    if (!pixelData || imageWidth <= 0 || imageHeight <= 0 || imageWidth > 1024 || imageHeight > 1024) return false;
    EnsureSlots();
    if (iconEntry->textureSlot < 0) {
        iconEntry->textureSlot = Gfx::TexAlloc();
        if (iconEntry->textureSlot < 0) return false;
    }
    return Gfx::TexUpload(iconEntry->textureSlot, pixelData, imageWidth, imageHeight, Gfx::TexFormat::RGBA);
}

bool Icons::UploadPng(const uint8_t *pngData, unsigned pngSize, bool downscale, int *textureSlotOut)
{
    EnsureSlots();
    if (!textureSlotOut) return false;
    *textureSlotOut = -1;
    int imageWidth = 0, imageHeight = 0;
    uint8_t *pixelData = DecodePng(pngData, pngSize, downscale, true, &imageWidth, &imageHeight);
    if (!pixelData) return false;
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

void Icons::Free(Entry *iconEntry)
{
    EnsureSlots();
    if (!iconEntry) return;
    if (iconEntry->textureSlot >= 0) {
        Gfx::TexFree(iconEntry->textureSlot);
        iconEntry->textureSlot = -1;
    }
    iconEntry->titleId = 0;
    iconEntry->isUsed = false;
    iconEntry->sequence = 0;
}

void Icons::PurgeHome()
{
    EnsureSlots();
    /* Keep icons cached (no purge) to prevent flicker on return to Home.
       The cache is bounded (kIconCacheCap), so memory stays bounded. */
    (void)0;
}

void Icons::TrimTitleIcons()
{
    EnsureSlots();
    for (auto &kv : s_state.titleCache) {
        if (kv.second.textureSlot >= 0)
            Gfx::TexFree(kv.second.textureSlot);
    }
    s_state.titleCache.clear();
    s_state.titleCache.rehash(0);
    for (int sslot : s_state.titleSlotPool) {
        Gfx::TexFree(sslot);
    }
    s_state.titleSlotPool.clear();
    s_state.titleSlotPool.shrink_to_fit();
    s_state.titleLive = 0;
}

bool Icons::TrimTitleIconsStep()
{
    EnsureSlots();
    if (s_state.titleCache.empty() && s_state.titleSlotPool.empty()) return false;
    if (!s_state.titleSlotPool.empty()) {
        int sslot = s_state.titleSlotPool.back();
        s_state.titleSlotPool.pop_back();
        Gfx::TexFree(sslot);
        return true;
    }
    auto oldest = s_state.titleCache.end();
    for (auto it = s_state.titleCache.begin(); it != s_state.titleCache.end(); ++it) {
        if (it->second.isMiss) continue;
        if (oldest == s_state.titleCache.end() || it->second.usedTick < oldest->second.usedTick)
            oldest = it;
    }
    if (oldest != s_state.titleCache.end() && oldest->second.textureSlot >= 0) {
        Gfx::TexFree(oldest->second.textureSlot);
        s_state.titleCache.erase(oldest);
        if (s_state.titleLive > 0) s_state.titleLive--;
        return true;
    }
    return false;
}
