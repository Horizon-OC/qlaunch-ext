/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Layout.hpp"
#include "Json.hpp"
#include "Gfx.hpp"
#include "Assets.hpp"
#include "App.hpp"
#include "Qext.hpp"
#include "../widgets/Widgets.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace Layout {

static LayoutNode nodes_[LAYOUT_MAX_NODES];
static int iconSlots_[GFX_ICONQ_MAX / 6];
static unsigned iconSlotCount_;
static int nodeCount_;

static void ClearNode(LayoutNode *n)
{
    memset(n, 0, sizeof(*n));
}

static float JNum(const JxDoc &d, int obj, const char *key, float dflt)
{
    int t = Jx::ObjGet(d, obj, key);
    if (Jx::TokType(d, t) != 4)
        return dflt;
    int len = 0;
    /* integer fast path via Jx::Num */
    return (float)Jx::Num(d, t, (long)dflt);
}

static bool JBool(const JxDoc &d, int obj, const char *key, bool dflt)
{
    int t = Jx::ObjGet(d, obj, key);
    if (Jx::TokType(d, t) != 4)
        return dflt;
    return Jx::IsTrue(d, t);
}

static void JStr(const JxDoc &d, int obj, const char *key, char *out,
                 unsigned cap)
{
    int t = Jx::ObjGet(d, obj, key);
    if (Jx::Str(d, t, out, cap) < 0 && cap)
        out[0] = 0;
}

bool LoadJson(const char *json, unsigned len)
{
    JxDoc doc;
    if (!Jx::Parse(doc, json, len))
        return false;
    if (Jx::TokType(doc, 0) != 1)
        return false;
    int ch = Jx::ObjGet(doc, 0, "children");
    if (Jx::TokType(doc, ch) != 2)
        return false;
    nodeCount_ = 0;
    int n = Jx::ArrSize(doc, ch);
    for (int i = 0; i < n && nodeCount_ < LAYOUT_MAX_NODES; i++) {
        int on = Jx::ArrAt(doc, ch, i);
        if (Jx::TokType(doc, on) != 1)
            continue;
        LayoutNode *nd = &nodes_[nodeCount_];
        ClearNode(nd);
        JStr(doc, on, "type", nd->type, sizeof(nd->type));
        JStr(doc, on, "id", nd->id, sizeof(nd->id));
        nd->x = JNum(doc, on, "x", 0.0f);
        nd->y = JNum(doc, on, "y", 0.0f);
        nd->tile = JNum(doc, on, "tile", 200.0f);
        nd->gap = JNum(doc, on, "gap", 24.0f);
        nd->label = JBool(doc, on, "label", false);
        int di = Jx::ObjGet(doc, on, "items");
        /* dock items: {glyph, action?}  hints: {button, label} */
        if (Jx::TokType(doc, di) == 2) {
            int m = Jx::ArrSize(doc, di);
            for (int k = 0; k < m; k++) {
                int io = Jx::ArrAt(doc, di, k);
                if (Jx::TokType(doc, io) != 1)
                    continue;
                if (nd->dockCount < LAYOUT_MAX_DOCK) {
                    int g = Jx::ObjGet(doc, io, "glyph");
                    if (Jx::TokType(doc, g) == 3) {
                        DockItem *it = &nd->dock[nd->dockCount++];
                        JStr(doc, io, "glyph", it->glyph, sizeof(it->glyph));
                        JStr(doc, io, "action", it->action, sizeof(it->action));
                        continue;
                    }
                }
                if (nd->hintCount < LAYOUT_MAX_HINTS) {
                    int b = Jx::ObjGet(doc, io, "button");
                    if (Jx::TokType(doc, b) == 3) {
                        HintItem *h = &nd->hints[nd->hintCount++];
                        JStr(doc, io, "button", h->button, sizeof(h->button));
                        JStr(doc, io, "label", h->label, sizeof(h->label));
                    }
                }
            }
        }
        nodeCount_++;
    }
    return nodeCount_ > 0;
}

bool LoadBuiltin()
{
    return LoadJson(layout_json, layout_json_size);
}

bool LoadFile(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
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

bool LoadAuto()
{
    if (LoadFile("sdmc:/qlaunch-ext/layout.json"))
        return true;
    return LoadBuiltin();
}

void BeginIconSlots()
{
    iconSlotCount_ = 0;
}

void PushIconSlot(int slot)
{
    if (iconSlotCount_ < (unsigned)(GFX_ICONQ_MAX / 6))
        iconSlots_[iconSlotCount_++] = slot;
}

void IconSlots(int *out)
{
    for (unsigned i = 0; i < (unsigned)(GFX_ICONQ_MAX / 6); i++)
        out[i] = i < iconSlotCount_ ? iconSlots_[i] : 0;
}

static const LayoutNode *FindDock()
{
    for (int i = 0; i < nodeCount_; i++) {
        if (strcmp(nodes_[i].type, "dock") == 0)
            return &nodes_[i];
    }
    return 0;
}

static int Actionable(const LayoutNode *nd, int *idx, int cap)
{
    int n = 0;
    for (int i = 0; i < nd->dockCount && n < cap; i++) {
        if (nd->dock[i].action[0])
            idx[n++] = i;
    }
    return n;
}

void ActivateDock()
{
    const LayoutNode *nd = FindDock();
    if (!nd)
        return;
    int idx[LAYOUT_MAX_DOCK];
    int n = Actionable(nd, idx, LAYOUT_MAX_DOCK);
    if (n <= 0)
        return;
    int ds = App::DockSel();
    if (ds < 0)
        ds = 0;
    if (ds >= n)
        ds = n - 1;
    const char *a = nd->dock[idx[ds]].action;
    if (strncmp(a, "applet:", 7) == 0)
        qext_launch_applet(atoi(a + 7));
}

void Draw()
{
    BeginIconSlots();
    for (int i = 0; i < nodeCount_; i++) {
        const LayoutNode &nd = nodes_[i];
        if (strcmp(nd.type, "background") == 0)
            WBackground::Draw(nd);
        else if (strcmp(nd.type, "topbar") == 0)
            WTopBar::Draw(nd);
        else if (strcmp(nd.type, "gamestrip") == 0)
            WStrip::Draw(nd);
        else if (strcmp(nd.type, "dock") == 0)
            WDock::Draw(nd);
        else if (strcmp(nd.type, "hints") == 0)
            WHints::Draw(nd);
    }
}

void Input(u64 down, u64 held)
{
    for (int i = 0; i < nodeCount_; i++) {
        const LayoutNode &nd = nodes_[i];
        if (strcmp(nd.type, "gamestrip") == 0)
            WStrip::Input(nd, down, held);
        else if (strcmp(nd.type, "dock") == 0)
            WDock::Input(nd, down, held);
    }
}

int NodeCount()
{
    return nodeCount_;
}

const LayoutNode &Node(int i)
{
    return nodes_[i < 0 ? 0 : i];
}

} /* namespace Layout */

