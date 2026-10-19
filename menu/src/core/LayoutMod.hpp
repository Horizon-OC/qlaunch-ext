/* qlaunch-ext layout module host (C) 2026 Souldbminer */
/* Licensed under the GPLv2                             */
#pragma once
#include <switch.h>
#include "Layout.hpp"

namespace LayoutMod {

    /// @brief Boot the active theme pack: its submenus, then its layout module.
    /// A design without submenus boots the layout alone.
    /// @return True when the layout (and its submenus, if any) loaded.
    bool Boot();

    /// @brief Shut down submenus, then unload the layout module.
    void Shutdown();

    /// @brief Draw chrome plus the active tab (dropped without layout).
    void Draw();

    /// @brief Route input to chrome or the active tab (dropped without layout).
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    void Input(u64 buttonsDown, u64 buttonsHeld);

    /// @brief Give the layout slide offset (0 without layout).
    /// @return Slide offset in pixels.
    float ShiftX();

    /// @brief Find a layout node by type (false without layout).
    /// @param type Node type string.
    /// @param nodeOut Output node (copied).
    /// @return True when found.
    bool FindNode(const char *type, LayoutNode *nodeOut);

    /// @brief Give the visible tab slot count (0 without layout).
    /// @return Slot count.
    int TabCount();

    /// @brief Resolve a visible slot to a Tabs registry index.
    /// @param slot Layout order 0..TabCount-1.
    /// @return Tabs index, or -1 when unresolvable.
    int TabAt(int slot);

    /// @brief Find the visible slot for a Tabs registry index.
    /// @param tab Tabs index.
    /// @return Layout slot, or -1 when hidden.
    int SlotOfTab(int tab);

} /* namespace LayoutMod */
