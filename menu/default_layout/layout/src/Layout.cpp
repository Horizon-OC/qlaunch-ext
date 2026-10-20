/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Layout.hpp"
#include "LayoutApi.hpp"
#include "LayoutHost.hpp"
#include "TouchNav.hpp"
#include "Json.hpp"
#include "Gfx.hpp"
#include "App.hpp"
#include "Sfx.hpp"
#include "Widgets.hpp"
#include "MenuHost.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace Layout {

    namespace {

        /// @brief Hardcoded chrome nodes (C++ driven until a format lands).
        static LayoutNode s_nodes[3] = {
            { "background", "", 0.0f, 0.0f, 0.0f, 0.0f, false, {}, 0, {}, 0 },
            { "topbar", "", 0.0f, 0.0f, 0.0f, 0.0f, false, {}, 0, {}, 0 },
            { "gamestrip", "games", 120.0f, 342.0f, 384.0f, 50.0f, true, {}, 0, {}, 0 },
        };

        /// @brief Parsed tab order backing store.
        LayoutDoc s_doc;

        /// @brief Host table installed by LayoutBoot.
        const MenuHostApi *s_layoutHost = nullptr;

        /// @brief Current horizontal slide offset for transitions.
        float s_slideOffsetX = 0.0f;

        /// @brief Read a string field into a fixed buffer.
        /// @param d JSON document.
        /// @param obj Object token.
        /// @param key Field name.
        /// @param out Output buffer.
        /// @param cap Buffer capacity.
        void JStr(const JxDoc &d, int obj, const char *key, char *out, unsigned cap)
        {
            int t = Jx::ObjGet(d, obj, key);

            if (Jx::Str(d, t, out, cap) < 0 && cap) {
                out[0] = 0;
            }
        }

        /// @brief Rebuild the tab order from the registry (builtin fallback).
        void DefaultTabs()
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            s_doc.tabCount = 0;

            int tabCount = hostApi->tabCount();

            for (int i = 0; i < tabCount && s_doc.tabCount < kLayoutMaxTabs; i++) {
                const char *tabId = hostApi->tabIdAt(i);

                if (!tabId || !tabId[0]) {
                    continue;
                }

                LayoutTab *slot = &s_doc.tabs[s_doc.tabCount++];
                snprintf(slot->id, sizeof(slot->id), "%s", tabId);
                slot->id[sizeof(slot->id) - 1] = 0;
                slot->tabIndex = i;
            }
        }

        /// @brief Fill tab slots from a "tabs" array token.
        ///
        /// Each entry is either a bare string id or an object with
        /// `id` plus an optional data-driven `jingle` clip name that
        /// overrides the tab's default via tabSetJingle().
        /// @param d JSON document.
        /// @param tabsArray Array token holding tab entries.
        void ParseTabEntries(const JxDoc &d, int tabsArray)
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            s_doc.tabCount = 0;

            int n = Jx::ArrSize(d, tabsArray);

            for (int i = 0; i < n && s_doc.tabCount < kLayoutMaxTabs; i++) {
                int to = Jx::ArrAt(d, tabsArray, i);
                char id[kLayoutIdMax] = { 0 };
                char jingle[kTabJingleMax] = { 0 };

                if (Jx::TokType(d, to) == 1) {
                    JStr(d, to, "id", id, sizeof(id));
                    JStr(d, to, "jingle", jingle, sizeof(jingle));
                } else {
                    Jx::Str(d, to, id, sizeof(id));
                }

                if (!id[0]) {
                    continue;
                }

                int idx = hostApi->tabFindById(id);

                if (idx < 0) {
                    continue;
                }

                if (jingle[0]) {
                    hostApi->tabSetJingle(idx, jingle);
                }

                LayoutTab *slot = &s_doc.tabs[s_doc.tabCount++];
                snprintf(slot->id, sizeof(slot->id), "%s", id);
                slot->id[sizeof(slot->id) - 1] = 0;
                slot->tabIndex = idx;
            }
        }

        /// @brief Load tab order from one TabOrder.json file.
        /// @param path Filesystem path.
        /// @return True when at least one tab resolved.
        bool LoadTabOrderFile(const char *path)
        {
            FILE *orderFile = fopen(path, "rb");

            if (!orderFile) {
                return false;
            }

            fseek(orderFile, 0, SEEK_END);
            long orderSize = ftell(orderFile);
            fseek(orderFile, 0, SEEK_SET);

            if (orderSize <= 0 || orderSize > 65536) {
                fclose(orderFile);
                return false;
            }

            char *orderBuf = (char *)malloc((unsigned)orderSize);

            if (!orderBuf) {
                fclose(orderFile);
                return false;
            }

            size_t orderGot = fread(orderBuf, 1, (unsigned)orderSize, orderFile);
            fclose(orderFile);

            if (orderGot != (unsigned)orderSize) {
                free(orderBuf);
                return false;
            }

            JxDoc orderDoc;

            if (!Jx::Parse(orderDoc, orderBuf, (unsigned)orderSize)) {
                free(orderBuf);
                return false;
            }

            free(orderBuf);

            if (Jx::TokType(orderDoc, 0) != 1) {
                return false;
            }

            int tabsArray = Jx::ObjGet(orderDoc, 0, "tabs");

            if (Jx::TokType(orderDoc, tabsArray) != 2) {
                return false;
            }

            ParseTabEntries(orderDoc, tabsArray);
            return s_doc.tabCount > 0;
        }

        /// @brief Draw one tab by registry index.
        /// @param tabIndex Tabs registry index.
        void DrawTab(int tabIndex)
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            bool isDynamicTab = hostApi->submenuIsDynamicTab(tabIndex) != 0;
            const char *tabId = hostApi->tabIdAt(tabIndex);

            if (!isDynamicTab && (!tabId || !tabId[0])) {
                return;
            }

            float backgroundRed = 1.0f;
            float backgroundGreen = 1.0f;
            float backgroundBlue = 1.0f;

            if (isDynamicTab) {
                if (!hostApi->submenuTabBackgroundColor(tabIndex, &backgroundRed, &backgroundGreen, &backgroundBlue)) {
                    backgroundRed = 1.0f;
                    backgroundGreen = 1.0f;
                    backgroundBlue = 1.0f;
                }
            } else if (tabId) {
                hostApi->themeTabColor(tabId, &backgroundRed, &backgroundGreen, &backgroundBlue);
            }

            hostApi->gfxPushPanel(s_slideOffsetX, 0.0f, 1920.0f, 1080.0f, 0.0f, backgroundRed, backgroundGreen, backgroundBlue, 1.0f);

            if (isDynamicTab) {
                hostApi->submenuDrawTab(tabIndex);
            }
        }

        /// @brief Route input to one tab by registry index.
        /// @param tabIndex Tabs registry index.
        /// @param buttonsDown Buttons pressed.
        /// @param buttonsHeld Buttons held.
        void InputTab(int tabIndex, u64 buttonsDown, u64 buttonsHeld)
        {
            const MenuHostApi *hostApi = GetLayoutHost();

            if (hostApi->submenuIsDynamicTab(tabIndex)) {
                hostApi->submenuInputTab(tabIndex, buttonsDown, buttonsHeld);
            }
        }

    } // namespace

    /// @brief Load the pack TabOrder.json, else registry order.
    /// Nodes are hardcoded (see s_nodes).
    /// @param packageDir Theme pack directory (may be null).
    void LoadTabOrder(const char *packageDir)
    {
        if (packageDir && packageDir[0]) {
            char orderPath[256];
            snprintf(orderPath, sizeof(orderPath), "%s/TabOrder.json", packageDir);
            orderPath[sizeof(orderPath) - 1] = 0;

            if (LoadTabOrderFile(orderPath)) {
                return;
            }
        }

        DefaultTabs();
    }

    /// @brief Give the current horizontal slide offset.
    /// @return Offset in pixels.
    float ShiftX()
    {
        return s_slideOffsetX;
    }

    /// @brief Install the host table for all layout widgets.
    /// @param hostApi Host API implemented by menu.dnro.
    void SetLayoutHost(const MenuHostApi *hostApi)
    {
        s_layoutHost = hostApi;
    }

    /// @brief Give the installed host table.
    /// @return Host API (never null after boot).
    const MenuHostApi *GetLayoutHost()
    {
        return s_layoutHost;
    }

    /// @brief Find a hardcoded node by type.
    /// @param type Node type string.
    /// @return Node pointer, or null when missing.
    const LayoutNode *FindNode(const char *type)
    {
        if (!type) {
            return NULL;
        }

        for (unsigned i = 0; i < sizeof(s_nodes) / sizeof(s_nodes[0]); i++) {
            if (strcmp(s_nodes[i].type, type) == 0) {
                return &s_nodes[i];
            }
        }

        return NULL;
    }

    /// @brief Draw background, active tab(s), top and bottom bars.
    void Draw()
    {
        const MenuHostApi *hostApi = GetLayoutHost();
        int fullTab = hostApi->submenuFullTabIndex();

        if (fullTab >= 0) {
            hostApi->gfxPushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            hostApi->gfxPushLayer((int)Gfx::GfxLayer::GameStrip);
            hostApi->submenuDrawTab(fullTab);
            hostApi->gfxPopLayer();
            return;
        }

        const LayoutNode *backgroundNode = FindNode("background");

        if (backgroundNode) {
            WBackground::Draw(*backgroundNode);
        }

        hostApi->gfxPushLayer((int)Gfx::GfxLayer::GameStrip);

        /* Transition curve and timing are menu-owned (menuSlideX);
           the layout only places the outgoing and incoming tabs. */
        if (hostApi->menuPhase() != MenuPhase_Animating || hostApi->menuT() >= 1.0f) {
            s_slideOffsetX = 0.0f;
            DrawTab(hostApi->appMenu());
        } else {
            float outgoingX = hostApi->menuSlideX();
            float slideDirection = (float)hostApi->menuDir();
            s_slideOffsetX = outgoingX;
            DrawTab(hostApi->menuFrom());
            s_slideOffsetX = outgoingX + slideDirection * 1920.0f;
            DrawTab(hostApi->appMenu());
            s_slideOffsetX = 0.0f;
        }

        hostApi->gfxPopLayer();

        const LayoutNode *topbarNode = FindNode("topbar");

        if (topbarNode) {
            WTopBar::Draw(*topbarNode);
        }

        WBottomBar::Draw();
    }

    /// @brief Route input to the focused layer, else the active tab.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    /// @param touches Touch frame (may be null when untouched).
    /// @param touchCount Touch point count.
    void Input(u64 buttonsDown, u64 buttonsHeld, const LayoutTouch *touches, int touchCount)
    {
        const MenuHostApi *hostApi = GetLayoutHost();
        TouchNav::Handle(touches, touchCount, buttonsDown);
        int fullTab = hostApi->submenuFullTabIndex();

        if (fullTab >= 0 && hostApi->menuT() >= 1.0f) {
            hostApi->submenuInputTab(fullTab, buttonsDown, buttonsHeld);
            return;
        }

        if ((buttonsDown & HidNpadButton_A) && !hostApi->popupIsOpen()) {
            if (hostApi->appActivate(buttonsHeld)) {
                buttonsDown &= ~HidNpadButton_A;
            }
        }

        if (buttonsDown & HidNpadButton_L) {
            if (!hostApi->popupIsOpen()) {
                hostApi->appTopSwitch(-1);
            }
        } else if (buttonsDown & HidNpadButton_R) {
            if (!hostApi->popupIsOpen()) {
                hostApi->appTopSwitch(1);
            }
        } else {
            int homeTabIndex = hostApi->tabFindById("home");

            if (hostApi->appMenu() == homeTabIndex && !hostApi->topFocus() && !hostApi->popupIsOpen()) {
                if (buttonsDown & HidNpadButton_X) {
                    if (hostApi->qextGameRunning()) {
                        hostApi->promptOpenGame(buttonsHeld, 0);
                    } else {
                        hostApi->sfxPlay((int)Sfx::Back);
                    }
                }

                if (buttonsDown & HidNpadButton_Plus) {
                    hostApi->refreshTitles();
                }
            }

            if (hostApi->topFocus()) {
                WTopBar::Input(buttonsDown, buttonsHeld);
            } else if (hostApi->bottomFocus()) {
                WBottomBar::Input(buttonsDown, buttonsHeld);
            } else {
                InputTab(hostApi->appMenu(), buttonsDown, buttonsHeld);
            }
        }
    }

    /// @brief Give the hardcoded node count.
    /// @return Node count.
    int NodeCount()
    {
        return (int)(sizeof(s_nodes) / sizeof(s_nodes[0]));
    }

    /// @brief Give a node by index (clamped).
    /// @param i Node index.
    /// @return Node reference.
    const LayoutNode &Node(int i)
    {
        int nodeCount = NodeCount();

        if (i < 0) {
            i = 0;
        }

        if (i >= nodeCount && nodeCount > 0) {
            i = nodeCount - 1;
        }

        return s_nodes[i < 0 ? 0 : i];
    }

    /// @brief Number of tabs in layout order.
    /// @return Tab slot count.
    int TabCount()
    {
        return s_doc.tabCount;
    }

    /// @brief Resolve a layout slot to a Tabs registry index.
    /// @param slot Layout order 0..TabCount-1.
    /// @return Tabs index, or -1 when unresolvable.
    int TabAt(int slot)
    {
        if (slot < 0 || slot >= s_doc.tabCount) {
            return -1;
        }

        return s_doc.tabs[slot].tabIndex;
    }

    /// @brief Find the layout slot for a Tabs registry index.
    /// @param tab Tabs index.
    /// @return Layout slot, or -1 when hidden.
    int SlotOfTab(int tab)
    {
        for (int i = 0; i < s_doc.tabCount; i++) {
            if (s_doc.tabs[i].tabIndex == tab) {
                return i;
            }
        }

        return -1;
    }

    /// @brief Direct access to the parsed tab order.
    /// @return Document reference.
    const LayoutDoc &Doc()
    {
        return s_doc;
    }

} /* namespace Layout */

/// @brief Install the host table for all layout widgets (global forwarder).
/// @param hostApi Host API implemented by menu.dnro.
void SetLayoutHost(const MenuHostApi *hostApi)
{
    Layout::SetLayoutHost(hostApi);
}

/// @brief Give the installed host table (global forwarder).
/// @return Host API (never null after boot).
const MenuHostApi *GetLayoutHost()
{
    return Layout::GetLayoutHost();
}

extern "C" {

    /// @brief Layout module API version.
    /// @return kLayoutApiVersion.
    QLAUNCH_HOOK int LayoutApiVersion(void)
    {
        return kLayoutApiVersion;
    }

    /// @brief Boot the layout with the menu host table.
    /// Nodes are hardcoded; only TabOrder.json loads from disk.
    /// @param hostApi Host API (never null).
    /// @param packageDir Theme pack directory owning TabOrder.json (may be null).
    /// @return 0 to stay loaded.
    QLAUNCH_HOOK int LayoutBoot(const MenuHostApi *hostApi, const char *packageDir)
    {
        if (!hostApi) {
            return 1;
        }

        Layout::SetLayoutHost(hostApi);
        Layout::LoadTabOrder(packageDir);
        return 0;
    }

    /// @brief Draw all chrome plus the active tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void LayoutDraw(const MenuHostApi *hostApi)
    {
        (void)hostApi;
        Layout::Draw();
    }

    /// @brief Route input to chrome or the active tab.
    /// @param hostApi Host API.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    /// @param touches Touch frame (may be null when untouched).
    /// @param touchCount Touch point count.
    QLAUNCH_HOOK void LayoutInput(const MenuHostApi *hostApi, unsigned long long buttonsDown,
                                  unsigned long long buttonsHeld, const LayoutTouch *touches,
                                  int touchCount)
    {
        (void)hostApi;
        Layout::Input(buttonsDown, buttonsHeld, touches, touchCount);
    }

    /// @brief Give the current horizontal slide offset.
    /// @param hostApi Host API.
    /// @return Slide offset in pixels.
    QLAUNCH_HOOK float LayoutShiftX(const MenuHostApi *hostApi)
    {
        (void)hostApi;
        return Layout::ShiftX();
    }

    /// @brief Find a layout node by type for tab content geometry.
    /// @param hostApi Host API.
    /// @param type Node type string.
    /// @param nodeOut Output node (copied).
    /// @return Non-zero when found.
    QLAUNCH_HOOK int LayoutFindNode(const MenuHostApi *hostApi, const char *type, LayoutNode *nodeOut)
    {
        (void)hostApi;
        const LayoutNode *foundNode = Layout::FindNode(type);

        if (!foundNode || !nodeOut) {
            return 0;
        }

        *nodeOut = *foundNode;
        return 1;
    }

    /// @brief Shut the layout module down before unload.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void LayoutShutdown(const MenuHostApi *hostApi)
    {
        (void)hostApi;
    }

    /// @brief Give the visible tab slot count from TabOrder.json.
    /// @param hostApi Host API.
    /// @return Slot count.
    QLAUNCH_HOOK int LayoutTabCount(const MenuHostApi *hostApi)
    {
        (void)hostApi;
        return Layout::TabCount();
    }

    /// @brief Resolve a visible slot to a Tabs registry index.
    /// @param hostApi Host API.
    /// @param slot Layout order 0..LayoutTabCount-1.
    /// @return Tabs index, or -1 when unresolvable.
    QLAUNCH_HOOK int LayoutTabAt(const MenuHostApi *hostApi, int slot)
    {
        (void)hostApi;
        return Layout::TabAt(slot);
    }

    /// @brief Find the visible slot for a Tabs registry index.
    /// @param hostApi Host API.
    /// @param tab Tabs index.
    /// @return Layout slot, or -1 when hidden.
    QLAUNCH_HOOK int LayoutSlotOfTab(const MenuHostApi *hostApi, int tab)
    {
        (void)hostApi;
        return Layout::SlotOfTab(tab);
    }

} /* extern C */
