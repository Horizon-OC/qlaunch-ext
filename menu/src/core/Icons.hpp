/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>
#include <deko3d.h>

/* Fixed LRU budget for title icons. Rows outside the budget keep
   fallback tiles until scrolling brings them near the focus. */
enum { ICONS_MAX = 16 };

enum {
    HUD_WIFI = 0,
    HUD_BATTERY,
    HUD_USER,
    HUD_BTN_A,
    HUD_BTN_PLUS,
    HUD_FOLDER,
    HUD_VGCCARD,
    HUD_TOPSEL_HOME,
    HUD_TOPSEL_CONNECT,
    HUD_TOPSEL_ESHOP,
    HUD_TOPSEL_ALBUM,
    HUD_TOPSEL_SETTINGS,
    HUD_TOPDES_HOME,
    HUD_TOPDES_CONNECT,
    HUD_TOPDES_ESHOP,
    HUD_TOPDES_ALBUM,
    HUD_TOPDES_SETTINGS,
    HUD_MARKER_STRIP,
    HUD_COUNT
};

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
        DkMemBlock mem;
        DkImage img;
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
