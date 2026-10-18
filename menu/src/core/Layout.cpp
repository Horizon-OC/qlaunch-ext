/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Layout.hpp"
#include "Json.hpp"
#include "Gfx.hpp"
#include "Assets.hpp"
#include "App.hpp"
#include "Sfx.hpp"
#include "Qext.hpp"
#include "Tabs.hpp"
#include "Submenus.hpp"
#include "../widgets/Widgets.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace Layout {

    namespace {

        /// @brief Parsed document backing store.
        LayoutDoc s_doc;

        /// @brief Current horizontal slide offset for transitions.
        float s_slideOffsetX = 0.0f;

        /// @brief Zero a node before parsing.
        /// @param n Node to clear.
        void ClearNode(LayoutNode *n)
        {
            memset(n, 0, sizeof(*n));
        }

        /// @brief Read a numeric field with a default.
        /// @param d JSON document.
        /// @param obj Object token.
        /// @param key Field name.
        /// @param dflt Default value.
        /// @return Numeric value or default.
        float JNum(const JxDoc &d, int obj, const char *key, float dflt)
        {
            int t = Jx::ObjGet(d, obj, key);

            if (Jx::TokType(d, t) != 4) {
                return dflt;
            }

            return (float)Jx::Num(d, t, (long)dflt);
        }

        /// @brief Read a boolean field with a default.
        /// @param d JSON document.
        /// @param obj Object token.
        /// @param key Field name.
        /// @param dflt Default value.
        /// @return Boolean value or default.
        bool JBool(const JxDoc &d, int obj, const char *key, bool dflt)
        {
            int t = Jx::ObjGet(d, obj, key);

            if (Jx::TokType(d, t) != 4) {
                return dflt;
            }

            return Jx::IsTrue(d, t);
        }

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

        /// @brief Parse one layout node object.
        /// @param jsonDoc JSON document.
        /// @param nodeToken Object token.
        /// @param targetNode Node to populate.
        void ParseNode(const JxDoc &jsonDoc, int nodeToken, LayoutNode *targetNode)
        {
            ClearNode(targetNode);
            JStr(jsonDoc, nodeToken, "type", targetNode->type, sizeof(targetNode->type));
            JStr(jsonDoc, nodeToken, "id", targetNode->id, sizeof(targetNode->id));
            targetNode->x = JNum(jsonDoc, nodeToken, "x", 0.0f);
            targetNode->y = JNum(jsonDoc, nodeToken, "y", 0.0f);
            targetNode->tile = JNum(jsonDoc, nodeToken, "tile", 200.0f);
            targetNode->gap = JNum(jsonDoc, nodeToken, "gap", 24.0f);
            targetNode->label = JBool(jsonDoc, nodeToken, "label", false);

            int itemsToken = Jx::ObjGet(jsonDoc, nodeToken, "items");

            /* Dock items: {glyph, action?}  Hints: {button, label}. */
            if (Jx::TokType(jsonDoc, itemsToken) != 2) {
                return;
            }

            int itemCount = Jx::ArrSize(jsonDoc, itemsToken);

            for (int itemIndex = 0; itemIndex < itemCount; itemIndex++) {
                int itemObject = Jx::ArrAt(jsonDoc, itemsToken, itemIndex);

                if (Jx::TokType(jsonDoc, itemObject) != 1) {
                    continue;
                }

                if (targetNode->dockCount < kLayoutMaxDock) {
                    int glyphToken = Jx::ObjGet(jsonDoc, itemObject, "glyph");

                    if (Jx::TokType(jsonDoc, glyphToken) == 3) {
                        DockItem *dockItem = &targetNode->dock[targetNode->dockCount++];
                        JStr(jsonDoc, itemObject, "glyph", dockItem->glyph, sizeof(dockItem->glyph));
                        JStr(jsonDoc, itemObject, "action", dockItem->action, sizeof(dockItem->action));
                        continue;
                    }
                }

                if (targetNode->hintCount < kLayoutMaxHints) {
                    int buttonToken = Jx::ObjGet(jsonDoc, itemObject, "button");

                    if (Jx::TokType(jsonDoc, buttonToken) == 3) {
                        HintItem *hintItem = &targetNode->hints[targetNode->hintCount++];
                        JStr(jsonDoc, itemObject, "button", hintItem->button, sizeof(hintItem->button));
                        JStr(jsonDoc, itemObject, "label", hintItem->label, sizeof(hintItem->label));
                    }
                }
            }
        }

        /// @brief Rebuild the tab order from the registry (builtin fallback).
        void DefaultTabs()
        {
            s_doc.tabCount = 0;

            for (int i = 0; i < Tabs::Count() && s_doc.tabCount < kLayoutMaxTabs; i++) {
                const MenuTab *tab = Tabs::Get(i);

                if (!tab) {
                    continue;
                }

                LayoutTab *slot = &s_doc.tabs[s_doc.tabCount++];
                snprintf(slot->id, sizeof(slot->id), "%s", tab->id);
                slot->id[sizeof(slot->id) - 1] = 0;
                slot->tabIndex = i;
            }
        }

        /// @brief Parse the optional "tabs" array; falls back to registry order.
        ///
        /// Each entry is either a bare string id or an object with
        /// `id` plus an optional data-driven `jingle` clip name that
        /// overrides the tab's default via Tabs::SetJingle().
        /// @param d JSON document.
        /// @param root Root object token.
        void ParseTabs(const JxDoc &d, int root)
        {
            s_doc.tabCount = 0;

            int ta = Jx::ObjGet(d, root, "tabs");

            if (Jx::TokType(d, ta) != 2) {
                DefaultTabs();
                return;
            }

            int n = Jx::ArrSize(d, ta);

            for (int i = 0; i < n && s_doc.tabCount < kLayoutMaxTabs; i++) {
                int to = Jx::ArrAt(d, ta, i);
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

                int idx = Tabs::FindById(id);

                if (idx < 0) {
                    continue;
                }

                if (jingle[0]) {
                    Tabs::SetJingle(idx, jingle);
                }

                LayoutTab *slot = &s_doc.tabs[s_doc.tabCount++];
                snprintf(slot->id, sizeof(slot->id), "%s", id);
                slot->id[sizeof(slot->id) - 1] = 0;
                slot->tabIndex = idx;
            }

            if (s_doc.tabCount <= 0) {
                DefaultTabs();
            }
        }

        /// @brief Draw one tab by registry index.
        /// @param tabIndex Tabs registry index.
        void DrawTab(int tabIndex)
        {
            const MenuTab *tabDescriptor = Tabs::Get(tabIndex);

            if (!tabDescriptor) {
                return;
            }

            float backgroundRed = 0.0f;
            float backgroundGreen = 0.0f;
            float backgroundBlue = 0.0f;

            if (Submenus::IsDynamicTab(tabIndex)) {
                if (!Submenus::TabBackgroundColor(tabIndex, &backgroundRed, &backgroundGreen, &backgroundBlue)) {
                    backgroundRed = 1.0f;
                    backgroundGreen = 1.0f;
                    backgroundBlue = 1.0f;
                }
            } else {
                Theme::MenuColor(Theme::MenuSlotForId(tabDescriptor->id), &backgroundRed, &backgroundGreen, &backgroundBlue);
            }

            Gfx::PushPanel(s_slideOffsetX, 0.0f, 1920.0f, 1080.0f, 0.0f, backgroundRed, backgroundGreen, backgroundBlue, 1.0f);

            if (strcmp(tabDescriptor->id, "home") == 0) {
                const LayoutNode *stripNode = FindNode("gamestrip");

                if (stripNode) {
                    WStrip::Draw(*stripNode);
                }

                WStrip::DrawSubs();
                return;
            }

            if (Submenus::IsDynamicTab(tabIndex)) {
                Submenus::DrawDynamicTab(tabIndex);
                return;
            }

            if (tabDescriptor->draw) {
                tabDescriptor->draw();
            }
        }

        /// @brief Route input to one tab by registry index.
        /// @param tabIndex Tabs registry index.
        /// @param buttonsDown Buttons pressed.
        /// @param buttonsHeld Buttons held.
        void InputTab(int tabIndex, u64 buttonsDown, u64 buttonsHeld)
        {
            const MenuTab *tabDescriptor = Tabs::Get(tabIndex);

            if (!tabDescriptor) {
                return;
            }

            if (strcmp(tabDescriptor->id, "home") == 0) {
                const LayoutNode *stripNode = FindNode("gamestrip");

                if (stripNode) {
                    WStrip::Input(*stripNode, buttonsDown, buttonsHeld);
                }

                return;
            }

            if (Submenus::IsDynamicTab(tabIndex)) {
                Submenus::InputDynamicTab(tabIndex, buttonsDown, buttonsHeld);
                return;
            }

            if (tabDescriptor->input) {
                tabDescriptor->input(buttonsDown, buttonsHeld);
            }
        }

    } // namespace

    /// @brief Load layout from a JSON buffer.
    /// @param json JSON text.
    /// @param len Text length.
    /// @return True when at least one node parsed.
    bool LoadJson(const char *json, unsigned len)
    {
        JxDoc doc;

        if (!Jx::Parse(doc, json, len)) {
            return false;
        }

        if (Jx::TokType(doc, 0) != 1) {
            return false;
        }

        memset(&s_doc, 0, sizeof(s_doc));
        s_doc.designW = 1920;
        s_doc.designH = 1080;

        int design = Jx::ObjGet(doc, 0, "design");

        if (Jx::TokType(doc, design) == 1) {
            s_doc.designW = (int)JNum(doc, design, "w", 1920.0f);
            s_doc.designH = (int)JNum(doc, design, "h", 1080.0f);
        }

        ParseTabs(doc, 0);

        int ch = Jx::ObjGet(doc, 0, "children");

        if (Jx::TokType(doc, ch) != 2) {
            return false;
        }

        int n = Jx::ArrSize(doc, ch);

        for (int i = 0; i < n && s_doc.nodeCount < kLayoutMaxNodes; i++) {
            int on = Jx::ArrAt(doc, ch, i);

            if (Jx::TokType(doc, on) != 1) {
                continue;
            }

            ParseNode(doc, on, &s_doc.nodes[s_doc.nodeCount++]);
        }

        return s_doc.nodeCount > 0;
    }

    /// @brief Load the builtin layout blob.
    /// @return True on success.
    bool LoadBuiltin()
    {
        return LoadJson(layout_json, layout_json_size);
    }

    /// @brief Load layout from an SD file.
    /// @param path Filesystem path.
    /// @return True on success.
    bool LoadFile(const char *path)
    {
        FILE *f = fopen(path, "rb");

        if (!f) {
            return false;
        }

        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (sz <= 0 || sz > 65536) {
            fclose(f);
            return false;
        }

        char *buf = (char *)malloc((unsigned)sz);

        if (!buf) {
            fclose(f);
            return false;
        }

        size_t got = fread(buf, 1, (unsigned)sz, f);
        fclose(f);

        bool ok = (got == (unsigned)sz) && LoadJson(buf, (unsigned)sz);
        free(buf);
        return ok;
    }

    /// @brief Load SD override, else builtin.
    /// @return True on success.
    bool LoadAuto()
    {
        if (LoadFile("sdmc:/qlaunch-ext/layout.json")) {
            return true;
        }

        return LoadBuiltin();
    }

    /// @brief Give the current horizontal slide offset.
    /// @return Offset in pixels.
    float ShiftX()
    {
        return s_slideOffsetX;
    }

    /// @brief Find the first node of a given type.
    /// @param type Node type string.
    /// @return Node pointer, or null when missing.
    const LayoutNode *FindNode(const char *type)
    {
        if (!type) {
            return NULL;
        }

        for (int i = 0; i < s_doc.nodeCount; i++) {
            if (strcmp(s_doc.nodes[i].type, type) == 0) {
                return &s_doc.nodes[i];
            }
        }

        return NULL;
    }

    /// @brief Activate the focused top-bar tab.
    void ActivateTop()
    {
        int m = App::TopSel();
        App::MenuJingle(m);
        App::SwitchMenu(m);
    }

    /// @brief Draw background, active tab(s), top and bottom bars.
    void Draw()
    {
        int fullTab = Submenus::FullTabIndex();

        if (fullTab >= 0) {
            Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            Submenus::DrawDynamicTab(fullTab);
            if (WPopup::IsOpen()) {
                WPopup::Draw();
            }
            return;
        }

        for (int nodeIndex = 0; nodeIndex < s_doc.nodeCount; nodeIndex++) {
            if (strcmp(s_doc.nodes[nodeIndex].type, "background") == 0) {
                WBackground::Draw(s_doc.nodes[nodeIndex]);
            }
        }

        float transitionProgress = App::MenuT();

        switch (App::MenuPhaseOf()) {
            case MenuPhase_Idle:
                s_slideOffsetX = 0.0f;
                DrawTab(App::Menu());
                break;

            case MenuPhase_Animating:
                if (transitionProgress >= 1.0f) {
                    s_slideOffsetX = 0.0f;
                    DrawTab(App::Menu());
                } else {
                    float easedOffset = 1.0f - (1.0f - transitionProgress) * (1.0f - transitionProgress) * (1.0f - transitionProgress);
                    int slideDirection = App::MenuDir();
                    s_slideOffsetX = -easedOffset * (float)slideDirection * 1920.0f;
                    DrawTab(App::MenuFrom());
                    s_slideOffsetX = (1.0f - easedOffset) * (float)slideDirection * 1920.0f;
                    DrawTab(App::Menu());
                    s_slideOffsetX = 0.0f;
                }
                break;
        }

        for (int nodeIndex = 0; nodeIndex < s_doc.nodeCount; nodeIndex++) {
            if (strcmp(s_doc.nodes[nodeIndex].type, "topbar") == 0) {
                WTopBar::Draw(s_doc.nodes[nodeIndex]);
            }
        }

        WBottomBar::Draw();

        if (WPopup::IsOpen()) {
            WPopup::Draw();
        }
    }

    /// @brief Route input to the focused layer, else the active tab.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void Input(u64 buttonsDown, u64 buttonsHeld)
    {
        if (WPrompt::IsOpen()) {
            WPrompt::Input(buttonsDown, buttonsHeld);
            return;
        }

        int fullTab = Submenus::FullTabIndex();

        if (fullTab >= 0 && App::MenuT() >= 1.0f) {
            Submenus::InputDynamicTab(fullTab, buttonsDown, buttonsHeld);
            return;
        }

        if (App::TopFocus()) {
            WTopBar::Input(buttonsDown, buttonsHeld);
            return;
        }

        if (App::BottomFocus()) {
            WBottomBar::Input(buttonsDown, buttonsHeld);
            return;
        }

        InputTab(App::Menu(), buttonsDown, buttonsHeld);
    }

    /// @brief Give the node count.
    /// @return Node count.
    int NodeCount()
    {
        return s_doc.nodeCount;
    }

    /// @brief Give a node by index (clamped).
    /// @param i Node index.
    /// @return Node reference.
    const LayoutNode &Node(int i)
    {
        if (i < 0) {
            i = 0;
        }

        if (i >= s_doc.nodeCount && s_doc.nodeCount > 0) {
            i = s_doc.nodeCount - 1;
        }

        return s_doc.nodes[i < 0 ? 0 : i];
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

    /// @brief Direct access to the parsed document.
    /// @return Document reference.
    const LayoutDoc &Doc()
    {
        return s_doc;
    }

} /* namespace Layout */
