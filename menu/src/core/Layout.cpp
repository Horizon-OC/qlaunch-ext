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
#include "../widgets/Widgets.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace Layout {

    namespace {

        /// @brief Parsed document backing store.
        LayoutDoc s_doc;

        /// @brief Current horizontal slide offset for transitions.
        float s_shiftX = 0.0f;

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
        /// @param d JSON document.
        /// @param on Object token.
        /// @param nd Node to populate.
        void ParseNode(const JxDoc &d, int on, LayoutNode *nd)
        {
            ClearNode(nd);
            JStr(d, on, "type", nd->type, sizeof(nd->type));
            JStr(d, on, "id", nd->id, sizeof(nd->id));
            nd->x = JNum(d, on, "x", 0.0f);
            nd->y = JNum(d, on, "y", 0.0f);
            nd->tile = JNum(d, on, "tile", 200.0f);
            nd->gap = JNum(d, on, "gap", 24.0f);
            nd->label = JBool(d, on, "label", false);

            int di = Jx::ObjGet(d, on, "items");

            /* Dock items: {glyph, action?}  Hints: {button, label}. */
            if (Jx::TokType(d, di) != 2) {
                return;
            }

            int m = Jx::ArrSize(d, di);

            for (int k = 0; k < m; k++) {
                int io = Jx::ArrAt(d, di, k);

                if (Jx::TokType(d, io) != 1) {
                    continue;
                }

                if (nd->dockCount < LAYOUT_MAX_DOCK) {
                    int g = Jx::ObjGet(d, io, "glyph");

                    if (Jx::TokType(d, g) == 3) {
                        DockItem *it = &nd->dock[nd->dockCount++];
                        JStr(d, io, "glyph", it->glyph, sizeof(it->glyph));
                        JStr(d, io, "action", it->action, sizeof(it->action));
                        continue;
                    }
                }

                if (nd->hintCount < LAYOUT_MAX_HINTS) {
                    int b = Jx::ObjGet(d, io, "button");

                    if (Jx::TokType(d, b) == 3) {
                        HintItem *h = &nd->hints[nd->hintCount++];
                        JStr(d, io, "button", h->button, sizeof(h->button));
                        JStr(d, io, "label", h->label, sizeof(h->label));
                    }
                }
            }
        }

        /// @brief Rebuild the tab order from the registry (builtin fallback).
        void DefaultTabs()
        {
            s_doc.tabCount = 0;

            for (int i = 0; i < Tabs::Count() && s_doc.tabCount < LAYOUT_MAX_TABS; i++) {
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

            for (int i = 0; i < n && s_doc.tabCount < LAYOUT_MAX_TABS; i++) {
                int to = Jx::ArrAt(d, ta, i);
                char id[LAYOUT_ID_MAX] = { 0 };
                char jingle[TAB_JINGLE_MAX] = { 0 };

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
        /// @param tab Tabs registry index.
        void DrawTab(int tab)
        {
            const MenuTab *desc = Tabs::Get(tab);

            if (!desc) {
                return;
            }

            float br, bg, bb;
            Theme::MenuColor(tab, &br, &bg, &bb);
            Gfx::PushPanel(s_shiftX, 0.0f, 1920.0f, 1080.0f, 0.0f, br, bg, bb, 1.0f);

            if (strcmp(desc->id, "home") == 0) {
                const LayoutNode *strip = FindNode("gamestrip");

                if (strip) {
                    WStrip::Draw(*strip);
                }

                WStrip::DrawSubs();
                return;
            }

            if (desc->draw) {
                desc->draw();
            }
        }

        /// @brief Route input to one tab by registry index.
        /// @param tab Tabs registry index.
        /// @param down Buttons pressed.
        /// @param held Buttons held.
        void InputTab(int tab, u64 down, u64 held)
        {
            const MenuTab *desc = Tabs::Get(tab);

            if (!desc) {
                return;
            }

            if (strcmp(desc->id, "home") == 0 || strcmp(desc->id, "album") == 0) {
                const LayoutNode *strip = FindNode("gamestrip");

                if (strip) {
                    WStrip::Input(*strip, down, held);
                }

                return;
            }

            if (desc->input) {
                desc->input(down, held);
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

        for (int i = 0; i < n && s_doc.nodeCount < LAYOUT_MAX_NODES; i++) {
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
        return s_shiftX;
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
        if (WAlbum::IsFull()) {
            Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            WAlbum::Draw();
            return;
        }

        for (int i = 0; i < s_doc.nodeCount; i++) {
            if (strcmp(s_doc.nodes[i].type, "background") == 0) {
                WBackground::Draw(s_doc.nodes[i]);
            }
        }

        float e = App::MenuT();

        if (e >= 1.0f) {
            s_shiftX = 0.0f;
            DrawTab(App::Menu());
        } else {
            float eo = 1.0f - (1.0f - e) * (1.0f - e) * (1.0f - e);
            int dir = App::MenuDir();
            s_shiftX = -eo * (float)dir * 1920.0f;
            DrawTab(App::MenuFrom());
            s_shiftX = (1.0f - eo) * (float)dir * 1920.0f;
            DrawTab(App::Menu());
            s_shiftX = 0.0f;
        }

        for (int i = 0; i < s_doc.nodeCount; i++) {
            if (strcmp(s_doc.nodes[i].type, "topbar") == 0) {
                WTopBar::Draw(s_doc.nodes[i]);
            }
        }

        WBottomBar::Draw();
    }

    /// @brief Route input to the focused layer, else the active tab.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    void Input(u64 down, u64 held)
    {
        if (WAlbum::IsOpen() && App::MenuT() >= 1.0f) {
            WAlbum::Input(down, held);
            return;
        }

        if (App::TopFocus()) {
            WTopBar::Input(down, held);
            return;
        }

        if (App::BottomFocus()) {
            WBottomBar::Input(down, held);
            return;
        }

        InputTab(App::Menu(), down, held);
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
