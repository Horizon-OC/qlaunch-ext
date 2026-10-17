/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>
#include <deko3d.h>

/* Fixed LRU budget for title icons. Rows outside the budget keep
   fallback tiles until scrolling brings them near the focus. */
constexpr int ICONS_MAX = 16;

constexpr int HUD_WIFI = 0;
constexpr int HUD_BATTERY = 1;
constexpr int HUD_USER = 2;
constexpr int HUD_BTN_A = 3;
constexpr int HUD_BTN_PLUS = 4;
constexpr int HUD_FOLDER = 5;
constexpr int HUD_VGCCARD = 6;
constexpr int HUD_TOPSEL_HOME = 7;
constexpr int HUD_TOPSEL_CONNECT = 8;
constexpr int HUD_TOPSEL_ESHOP = 9;
constexpr int HUD_TOPSEL_ALBUM = 10;
constexpr int HUD_TOPSEL_SETTINGS = 11;
constexpr int HUD_TOPDES_HOME = 12;
constexpr int HUD_TOPDES_CONNECT = 13;
constexpr int HUD_TOPDES_ESHOP = 14;
constexpr int HUD_TOPDES_ALBUM = 15;
constexpr int HUD_TOPDES_SETTINGS = 16;
constexpr int HUD_MARKER_STRIP = 17;
constexpr int HUD_COUNT = 18;

struct IconReq {
    u64 tid;
    int rowIndex; /* row to write the slot into */
};

namespace Icons {
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

    /// @brief Load HUD icons for the current theme.
    /// @return True on success.
    bool InitHud();

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
