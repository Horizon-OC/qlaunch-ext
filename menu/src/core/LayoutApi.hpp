/* qlaunch-ext layout module API (C) 2026 Souldbminer */
/* Licensed under the GPLv2                          */

#pragma once
#include "Layout.hpp"

#ifndef QLAUNCH_HOOK
#define QLAUNCH_HOOK __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

    enum {
        kLayoutApiVersion = 1
    };

    struct MenuHostApi;

    /// @brief Layout module API version (must equal kLayoutApiVersion).
    QLAUNCH_HOOK int LayoutApiVersion(void);

    /// @brief Boot the layout module (chrome, focus routing, tab dispatch).
    /// @param hostApi Host API implemented by menu.dnro (never null).
    /// @param packageDir Package directory owning layout.json (may be null).
    /// @return 0 to stay loaded.
    QLAUNCH_HOOK int LayoutBoot(const struct MenuHostApi *hostApi, const char *packageDir);

    /// @brief Draw all chrome plus the active tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void LayoutDraw(const struct MenuHostApi *hostApi);

    /// @brief Route input to chrome or the active tab.
    /// @param hostApi Host API.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    QLAUNCH_HOOK void LayoutInput(const struct MenuHostApi *hostApi, unsigned long long buttonsDown,
                                  unsigned long long buttonsHeld);

    /// @brief Shut the layout module down before unload.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void LayoutShutdown(const struct MenuHostApi *hostApi);

    /// @brief Give the current horizontal slide offset for tab transitions.
    /// @param hostApi Host API.
    /// @return Slide offset in pixels (0 when idle).
    QLAUNCH_HOOK float LayoutShiftX(const struct MenuHostApi *hostApi);

    /// @brief Find a layout node by type for tab content geometry.
    /// @param hostApi Host API.
    /// @param type Node type string (for example "gamestrip").
    /// @param nodeOut Output node (copied).
    /// @return Non-zero when found.
    QLAUNCH_HOOK int LayoutFindNode(const struct MenuHostApi *hostApi, const char *type,
                                    struct LayoutNode *nodeOut);

    /// @brief Give the visible tab slot count from TabOrder.json.
    /// @param hostApi Host API.
    /// @return Slot count.
    QLAUNCH_HOOK int LayoutTabCount(const struct MenuHostApi *hostApi);

    /// @brief Resolve a visible slot to a Tabs registry index.
    /// @param hostApi Host API.
    /// @param slot Layout order 0..LayoutTabCount-1.
    /// @return Tabs index, or -1 when unresolvable.
    QLAUNCH_HOOK int LayoutTabAt(const struct MenuHostApi *hostApi, int slot);

    /// @brief Find the visible slot for a Tabs registry index.
    /// @param hostApi Host API.
    /// @param tab Tabs index.
    /// @return Layout slot, or -1 when hidden.
    QLAUNCH_HOOK int LayoutSlotOfTab(const struct MenuHostApi *hostApi, int tab);

#ifdef __cplusplus
} /* extern C */
#endif
