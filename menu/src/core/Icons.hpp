/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>
#include <deko3d.h>

/* Fixed LRU budget for title icons. Rows outside the budget keep
   fallback tiles until scrolling brings them near the focus. */
constexpr int kIconsMax = 16;

constexpr int kHudUser = 0;
constexpr int kHudButtonA = 1;
constexpr int kHudButtonPlus = 2;
constexpr int kHudFolder = 3;
constexpr int kHudVgcCard = 4;
constexpr int kHudTopSelHome = 5;
constexpr int kHudTopSelConnect = 6;
constexpr int kHudTopSelEshop = 7;
constexpr int kHudTopSelAlbum = 8;
constexpr int kHudTopSelSettings = 9;
constexpr int kHudTopDesHome = 10;
constexpr int kHudTopDesConnect = 11;
constexpr int kHudTopDesEshop = 12;
constexpr int kHudTopDesAlbum = 13;
constexpr int kHudTopDesSettings = 14;
constexpr int kHudMarkerStrip = 15;
constexpr int kHudWifi0 = 16;
constexpr int kHudWifi1 = 17;
constexpr int kHudWifi2 = 18;
constexpr int kHudWifi3 = 19;
constexpr int kHudWifiNone = 20;
constexpr int kHudAirplane = 21;
constexpr int kHudCount = 22;

struct IconReq {
    u64 titleId;
    int rowIndex; /* row to write the slot into */
};

namespace Icons {
    /// @brief One HUD icon asset for the engine to decode and upload.
    /// New icons only need a row in the caller's table (index = kHud* id).
    struct HudDef {
        const uint8_t *png;   ///< PNG bytes (bin2s symbol).
        const uint32_t *size; ///< PNG byte size (bin2s symbol).
        bool noInvert;        ///< Keep native colors on dark themes.
    };

    /// @brief Sync title icon slots for the given title ids.
    /// Rows nearest the focus are preferred, so the visible window
    /// owns the fixed budget and scrolling rebalances it. Uncached
    /// rows keep slot -1 (fallback tile) until a later pass loads
    /// them; at most a full budget of icons loads per call.
    /// @param titleIds Title ids per row.
    /// @param outputSlots Output slots per row.
    /// @param rowCount Row count.
    /// @param focusedRow Selected row the budget centers on.
    void Sync(const u64 *titleIds, int *outputSlots, int rowCount, int focusedRow);

    /// @brief Free all title icons.
    void FreeAll();

    /// @brief Drop cached HUD icons (theme change).
    void ResetHud();

    /// @brief Load HUD icons for the current theme from a caller table.
    /// @param assetDefs Asset table (index = kHud* id), or null for none.
    /// @param assetCount Table length (clamped to kHudCount).
    /// @return True when at least one icon loaded.
    bool InitHud(const HudDef *assetDefs, int assetCount);

    /// @brief Look up a HUD icon slot.
    /// @param hudId kHud* id.
    /// @return Icon slot, or <= 0 when missing.
    int HudSlot(int hudId);

    struct Entry {
        u64 titleId;
        bool isUsed;
        u64 sequence;
        int textureSlot;
    };

    /// @brief Upload RGBA pixels into an icon entry.
    /// @param iconEntry Entry receiving the image.
    /// @param pixelData Pixel data.
    /// @param imageWidth Width.
    /// @param imageHeight Height.
    /// @return True on success.
    bool Upload(Entry *iconEntry, const uint8_t *pixelData, int imageWidth, int imageHeight);

    /// @brief Decode a PNG and upload it to a fresh texture slot.
    /// @param pngData PNG bytes.
    /// @param pngSize Byte size (64KB max).
    /// @param downscale True to shrink art larger than 256px, like core art.
    /// @param textureSlotOut Output texture slot, or -1 on failure.
    /// @return True on success.
    bool UploadPng(const uint8_t *pngData, unsigned pngSize, bool downscale, int *textureSlotOut);

    /// @brief Free an icon entry's GPU resources.
    /// @param iconEntry Entry to free.
    void Free(Entry *iconEntry);
} /* namespace Icons */
