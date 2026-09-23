/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/App.hpp"
#include <string.h>

namespace WDock {

static unsigned GlyphFor(const char *name)
{
    if (strcmp(name, "chat") == 0)
        return GLYPH_CHAT;
    if (strcmp(name, "news") == 0)
        return GLYPH_NEWS;
    if (strcmp(name, "eshop") == 0)
        return GLYPH_ESHOP;
    if (strcmp(name, "album") == 0)
        return GLYPH_ALBUM;
    if (strcmp(name, "controllers") == 0)
        return GLYPH_JOYCON;
    if (strcmp(name, "settings") == 0)
        return GLYPH_GEAR;
    if (strcmp(name, "power") == 0)
        return GLYPH_POWER;
    return 0;
}

static void GlyphColor(const char *name, float *r, float *g, float *b)
{
    if (strcmp(name, "chat") == 0) {
        *r = 0.95f;
        *g = 0.60f;
        *b = 0.10f;
    } else if (strcmp(name, "news") == 0) {
        *r = 0.20f;
        *g = 0.65f;
        *b = 0.35f;
    } else if (strcmp(name, "eshop") == 0) {
        *r = 1.0f;
        *g = 0.42f;
        *b = 0.05f;
    } else if (strcmp(name, "album") == 0) {
        *r = 0.29f;
        *g = 0.56f;
        *b = 0.85f;
    } else {
        *r = 0.55f;
        *g = 0.55f;
        *b = 0.60f;
    }
}

static int Actionable(const LayoutNode &nd, int *idx, int cap)
{
    int n = 0;
    for (int i = 0; i < nd.dockCount && n < cap; i++) {
        if (nd.dock[i].action[0])
            idx[n++] = i;
    }
    return n;
}

void Draw(const LayoutNode &nd)
{
    if (nd.dockCount <= 0)
        return;
    float dockR, dockG, dockB;
    Theme::Color("dock", &dockR, &dockG, &dockB);
    float accR, accG, accB;
    Theme::Color("selring", &accR, &accG, &accB);
    float item = 76.0f, pitch = 116.0f;
    float total = (float)nd.dockCount * item + (float)(nd.dockCount - 1) * (pitch - item);
    float x0 = 960.0f - total * 0.5f;
    float h = Theme::Metric("dock_h", 110.0f);
    Gfx::PushPanel(x0 - 34.0f, nd.y, total + 68.0f, h, h * 0.5f,
                   dockR, dockG, dockB, 1.0f);

    int idx[LAYOUT_MAX_DOCK];
    int nAct = Actionable(nd, idx, LAYOUT_MAX_DOCK);
    int focusPos = -1;
    if (App::DockFocus()) {
        int ds = App::DockSel();
        if (ds >= 0 && ds < nAct)
            focusPos = idx[ds];
    }
    for (int i = 0; i < nd.dockCount; i++) {
        float cx = x0 + (float)i * pitch + item * 0.5f;
        float cy = nd.y + h * 0.5f;
        if (i == focusPos)
            Gfx::PushPanel(cx - 48.0f, cy - 48.0f, 96.0f, 96.0f, 48.0f,
                           accR, accG, accB, 1.0f);
        if (strcmp(nd.dock[i].glyph, "nso") == 0) {
            Gfx::PushPanel(cx - 34.0f, cy - 34.0f, 68.0f, 68.0f, 34.0f,
                           0.90f, 0.0f, 0.07f, 1.0f);
            char gb[4] = { 0 };
            GlyphUTF8(GLYPH_JOYCON, gb);
            float gw = Font::Measure(gb, 40.0f);
            Font::Draw(gb, cx - gw * 0.5f, cy + 14.0f, 40.0f,
                       1.0f, 1.0f, 1.0f);
        } else {
            unsigned cp = GlyphFor(nd.dock[i].glyph);
            if (!cp)
                continue;
            float cr, cg, cb;
            GlyphColor(nd.dock[i].glyph, &cr, &cg, &cb);
            char gb[4] = { 0 };
            GlyphUTF8(cp, gb);
            float gw = Font::Measure(gb, 64.0f);
            Font::Draw(gb, cx - gw * 0.5f, cy + 22.0f, 64.0f, cr, cg, cb);
        }
    }
}

void Input(const LayoutNode &nd, u64 down, u64 held)
{
    (void)held;
    if (!App::DockFocus())
        return;
    int idx[LAYOUT_MAX_DOCK];
    int n = Actionable(nd, idx, LAYOUT_MAX_DOCK);
    if (n <= 0) {
        App::SetDockFocus(false);
        return;
    }
    int ds = App::DockSel();
    if (down & HidNpadButton_AnyLeft) {
        ds--;
        if (ds < 0)
            ds = 0;
        App::SetDockSel(ds);
    }
    if (down & HidNpadButton_AnyRight) {
        ds++;
        if (ds >= n)
            ds = n - 1;
        App::SetDockSel(ds);
    }
    if (down & HidNpadButton_AnyUp)
        App::SetDockFocus(false);
}

} /* namespace WDock */

