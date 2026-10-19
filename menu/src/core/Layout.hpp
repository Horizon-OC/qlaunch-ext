/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

constexpr int kLayoutMaxNodes = 48;
constexpr int kLayoutMaxDock = 12;
constexpr int kLayoutMaxHints = 8;
constexpr int kLayoutMaxTabs = 12;
constexpr int kLayoutIdMax = 24;

/// @brief One dock shortcut inside a node.
struct DockItem {
    char glyph[16];
    char action[24];
};

/// @brief One footer hint inside a node.
struct HintItem {
    char button[8];
    char label[48];
};

/// @brief One generic layout node parsed from JSON.
struct LayoutNode {
    char type[16];
    char id[24];
    float x, y, tile, gap;
    bool label;
    DockItem dock[kLayoutMaxDock];
    int dockCount;
    HintItem hints[kLayoutMaxHints];
    int hintCount;
};

/// @brief Ordered tab reference resolved from layout.json.
struct LayoutTab {
    char id[kLayoutIdMax];
    int tabIndex; /* Index into the Tabs registry, -1 when missing. */
};

/// @brief Parsed layout document: nodes + tab order.
struct LayoutDoc {
    LayoutNode nodes[kLayoutMaxNodes];
    int nodeCount;
    LayoutTab tabs[kLayoutMaxTabs];
    int tabCount;
    int designW;
    int designH;
};

namespace Layout {
    /// @brief Draw background, active tab(s), top and bottom bars.
    void Draw();

    /// @brief Route input to the focused layer, else the active tab.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void Input(u64 buttonsDown, u64 buttonsHeld);

    /// @brief Give the current horizontal slide offset.
    /// @return Offset in pixels.
    float ShiftX();

    /// @brief Give the node count.
    /// @return Node count.
    int NodeCount();

    /// @brief Give a node by index (clamped).
    /// @param i Node index.
    /// @return Node reference.
    const LayoutNode &Node(int i);

    /// @brief Find the first node of a given type.
    /// @param type Node type string.
    /// @return Node pointer, or null when missing.
    const LayoutNode *FindNode(const char *type);

    /// @brief Number of tabs in layout order.
    /// @return Tab slot count.
    int TabCount();

    /// @brief Resolve a layout slot to a Tabs registry index.
    /// @param slot Layout order 0..TabCount-1.
    /// @return Tabs index, or -1 when unresolvable.
    int TabAt(int slot);

    /// @brief Find the layout slot for a Tabs registry index.
    /// @param tab Tabs index.
    /// @return Layout slot, or -1 when hidden.
    int SlotOfTab(int tab);

    /// @brief Direct access to the parsed document.
    /// @return Document reference.
    const LayoutDoc &Doc();
} /* namespace Layout */
