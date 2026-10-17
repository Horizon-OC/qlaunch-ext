/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>
#include <deko3d.h>

/* Fixed LRU budget for title icons. Rows outside the budget keep
   fallback tiles until scrolling brings them near the focus. */
constexpr int ICONS_MAX = 16;

constexpr int HUD_USER = 0;
constexpr int HUD_BTN_A = 1;
constexpr int HUD_BTN_PLUS = 2;
constexpr int HUD_FOLDER = 3;
constexpr int HUD_VGCCARD = 4;
constexpr int HUD_TOPSEL_HOME = 5;
constexpr int HUD_TOPSEL_CONNECT = 6;
constexpr int HUD_TOPSEL_ESHOP = 7;
constexpr int HUD_TOPSEL_ALBUM = 8;
constexpr int HUD_TOPSEL_SETTINGS = 9;
constexpr int HUD_TOPDES_HOME = 10;
constexpr int HUD_TOPDES_CONNECT = 11;
constexpr int HUD_TOPDES_ESHOP = 12;
constexpr int HUD_TOPDES_ALBUM = 13;
constexpr int HUD_TOPDES_SETTINGS = 14;
constexpr int HUD_MARKER_STRIP = 15;
constexpr int HUD_WIFI_0 = 16;
constexpr int HUD_WIFI_1 = 17;
constexpr int HUD_WIFI_2 = 18;
constexpr int HUD_WIFI_3 = 19;
constexpr int HUD_WIFI_NONE = 20;
constexpr int HUD_AIRPLANE = 21;
constexpr int HUD_COUNT = 22;

struct IconReq {
    u64 tid;
    int rowIndex; /* row to write the slot into */
};

namespace Icons {
    /// @brief One HUD icon asset for the engine to decode and upload.
    /// New icons only need a row in the caller's table (index = HUD_* id).
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
    /// @param tids Title ids per row.
    /// @param slots Output slots per row.
    /// @param n Row count.
    /// @param focus Selected row the budget centers on.
    void Sync(const u64 *tids, int *slots, int n, int focus);

    /// @brief Free all title icons.
    void FreeAll();

    /// @brief Drop cached HUD icons (theme change).
    void ResetHud();

    /// @brief Load HUD icons for the current theme from a caller table.
    /// @param defs Asset table (index = HUD_* id), or null for none.
    /// @param count Table length (clamped to HUD_COUNT).
    /// @return True when at least one icon loaded.
    bool InitHud(const HudDef *defs, int count);

    /// @brief Look up a HUD icon slot.
    /// @param which HUD_* id.
    /// @return Icon slot, or <= 0 when missing.
    int HudSlot(int which);

    struct Entry {
        u64 tid;
        bool used;
        u64 seq;
        int slot;
    };

    /// @brief Upload RGBA pixels into an icon entry.
    /// @param e Entry receiving the image.
    /// @param rgba Pixel data.
    /// @param w Width.
    /// @param h Height.
    /// @return True on success.
    bool Upload(Entry *e, const uint8_t *rgba, int w, int h);

    /// @brief Free an icon entry's GPU resources.
    /// @param e Entry to free.
    void Free(Entry *e);
} /* namespace Icons */
