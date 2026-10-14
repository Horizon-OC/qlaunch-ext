/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

constexpr int LAYOUT_MAX_NODES = 48;
constexpr int LAYOUT_MAX_DOCK = 12;
constexpr int LAYOUT_MAX_HINTS = 8;
constexpr int LAYOUT_MAX_TABS = 8;
constexpr int LAYOUT_ID_MAX = 24;

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
    DockItem dock[LAYOUT_MAX_DOCK];
    int dockCount;
    HintItem hints[LAYOUT_MAX_HINTS];
    int hintCount;
};

/// @brief Ordered tab reference resolved from layout.json.
struct LayoutTab {
    char id[LAYOUT_ID_MAX];
    int tabIndex; /* Index into the Tabs registry, -1 when missing. */
};

/// @brief Parsed layout document: nodes + tab order.
struct LayoutDoc {
    LayoutNode nodes[LAYOUT_MAX_NODES];
    int nodeCount;
    LayoutTab tabs[LAYOUT_MAX_TABS];
    int tabCount;
    int designW;
    int designH;
};

namespace Layout {
    /// @brief Load SD override, else builtin.
    /// @return True on success.
    bool LoadAuto();

    /// @brief Load layout from a JSON buffer.
    /// @param json JSON text.
    /// @param len Text length.
    /// @return True when at least one node parsed.
    bool LoadJson(const char *json, unsigned len);

    /// @brief Load the builtin layout blob.
    /// @return True on success.
    bool LoadBuiltin();

    /// @brief Load layout from an SD file.
    /// @param path Filesystem path.
    /// @return True on success.
    bool LoadFile(const char *path);

    /// @brief Draw background, active tab(s), top and bottom bars.
    void Draw();

    /// @brief Route input to the focused layer, else the active tab.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    void Input(u64 down, u64 held);

    /// @brief Activate the focused top-bar tab.
    void ActivateTop();

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
