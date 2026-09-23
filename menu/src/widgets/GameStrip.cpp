/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Layout.hpp"
#include "../core/App.hpp"
#include "../core/Qext.hpp"
#include <string.h>

namespace WStrip {

static float s_scroll = 0.0f;
static unsigned s_rep = 0;

static void TileFallback(const AppRow &r, float x, float y, float size,
                         float rad)
{
    float cr = 0.5f, cg = 0.5f, cb = 0.55f;
    unsigned glyph = 0;
    if (r.isGame) {
        uint32_t h = (uint32_t)(r.tid ^ (r.tid >> 32));
        h ^= h >> 16;
        h *= 0x7feb352d;
        h ^= h >> 15;
        h *= 0x846ca68b;
        h ^= h >> 16;
        cr = 0.35f + 0.55f * ((float)((h >> 0) & 0xFF) / 255.0f);
        cg = 0.35f + 0.55f * ((float)((h >> 8) & 0xFF) / 255.0f);
        cb = 0.35f + 0.55f * ((float)((h >> 16) & 0xFF) / 255.0f);
    } else if (r.applet == 0) {
        Theme::Color("tile_album", &cr, &cg, &cb);
        glyph = GLYPH_ALBUM;
    } else {
        Theme::Color("tile_ctrl", &cr, &cg, &cb);
        glyph = GLYPH_JOYCON;
    }
    Gfx::PushPanel(x, y, size, size, rad, cr, cg, cb, 1.0f);
    if (glyph) {
        char gb[4] = { 0 };
        GlyphUTF8(glyph, gb);
        float gs = size * 0.45f;
        float gw = Font::Measure(gb, gs);
        Font::Draw(gb, x + (size - gw) * 0.5f, y + size * 0.5f + gs * 0.35f,
                   gs, 1.0f, 1.0f, 1.0f);
    } else {
        const char *p = r.name;
        uint32_t cp = Font::Next(&p);
        if (cp < 32u || cp > 255u)
            cp = 63u;
        char ib[8] = { 0 };
        if (cp < 0x80) {
            ib[0] = (char)cp;
        } else if (cp < 0x800) {
            ib[0] = (char)(0xC0 | (cp >> 6));
            ib[1] = (char)(0x80 | (cp & 0x3F));
        } else {
            ib[0] = 63;
        }
        float ls = size * 0.42f;
        float lw = Font::Measure(ib, ls);
        Font::Draw(ib, x + (size - lw) * 0.5f,
                   y + size * 0.5f + ls * 0.35f, ls, 1.0f, 1.0f, 1.0f);
    }
}

void Draw(const LayoutNode &nd)
{
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float corner = Theme::Metric("corner", 30.0f);
    float frame = Theme::Metric("frame", 8.0f);
    float tile = nd.tile > 0.0f ? nd.tile : 300.0f;
    float gap = nd.gap > 0.0f ? nd.gap : 36.0f;
    float pitch = tile + gap;

    int n = App::RowCount();
    int sel = App::Sel();
    u64 susp = qext_suspended_title();
    bool running = susp != 0 && qext_game_running();

    if (nd.label && sel >= 0 && sel < n) {
        float ar, ag, ab;
        Theme::Color("accent", &ar, &ag, &ab);
        Gfx::PushPanel(nd.x, nd.y - 84.0f, 18.0f, 18.0f, 4.0f, ar, ag, ab, 1.0f);
        Font::Draw(App::Row(sel).name, nd.x + 30.0f, nd.y - 48.0f, 44.0f,
                   inkR, inkG, inkB);
    }

    /* Ease scroll so the selection stays on screen. */
    float total = (float)n * pitch;
    float selCx = (float)sel * pitch + tile * 0.5f;
    float target = selCx - 960.0f;
    float maxS = total - 1920.0f + 128.0f;
    if (maxS < 0.0f)
        maxS = 0.0f;
    if (target < 0.0f)
        target = 0.0f;
    if (target > maxS)
        target = maxS;
    s_scroll += (target - s_scroll) * 0.18f;
    if (s_scroll < 0.0f)
        s_scroll = 0.0f;

    float accR, accG, accB;
    Theme::Color("selring", &accR, &accG, &accB);
    int k0 = (int)(s_scroll / pitch) - 1;
    if (k0 < 0)
        k0 = 0;
    int k1 = (int)((s_scroll + 1920.0f) / pitch) + 2;
    if (k1 > n + 2)
        k1 = n + 2;
    for (int k = k0; k < k1; k++) {
        float cx = nd.x + (float)k * pitch - s_scroll + tile * 0.5f;
        if (k < 0 || k >= n) {
            /* Empty slot placeholder. */
            float ex = nd.x + (float)k * pitch - s_scroll;
            if (ex + tile < -50.0f || ex > 1970.0f)
                continue;
            float pr, pg, pb;
            Theme::Color("panel", &pr, &pg, &pb);
            Gfx::PushPanel(ex, nd.y, tile, tile, corner, pr, pg, pb, 1.0f);
            continue;
        }
        const AppRow &r = App::Row(k);
        bool active = r.isGame && running && r.tid == susp;
        bool selected = (k == sel) && !App::DockFocus();
        float size = tile;
        if (active)
            size = tile * 1.15f;
        else if (selected)
            size = tile * 1.08f;
        float x = cx - size * 0.5f;
        float y = nd.y + (tile - size) * 0.5f;
        if (x + size < -50.0f || x > 1970.0f)
            continue;
        float pad = frame * (size / tile);
        if (selected || active)
            Gfx::PushPanel(x - pad - 4.0f, y - pad - 4.0f, size + (pad + 4.0f) * 2.0f,
                           size + (pad + 4.0f) * 2.0f, corner,
                           accR, accG, accB, 1.0f);
        if (r.iconSlot > 0) {
            float fr, fg, fb;
            Theme::Color("panel", &fr, &fg, &fb);
            Gfx::PushPanel(x - pad, y - pad, size + pad * 2.0f, size + pad * 2.0f,
                           corner, fr, fg, fb, 1.0f);
            unsigned q = Gfx::PushIcon(x, y, size, size, corner * (size / tile),
                                       1.0f, 1.0f, 1.0f);
            if (q < (unsigned)(GFX_ICONQ_MAX / 6))
                Layout::PushIconSlot(r.iconSlot);
            else
                TileFallback(r, x, y, size, corner * (size / tile));
        } else {
            float fr, fg, fb;
            Theme::Color("panel", &fr, &fg, &fb);
            Gfx::PushPanel(x - pad, y - pad, size + pad * 2.0f, size + pad * 2.0f,
                           corner, fr, fg, fb, 1.0f);
            TileFallback(r, x, y, size, corner * (size / tile));
        }
    }
}

void Input(const LayoutNode &nd, u64 down, u64 held)
{
    (void)nd;
    if (App::DockFocus())
        return;
    bool moved = false;
    if (down & HidNpadButton_AnyLeft) {
        App::MoveSel(-1);
        moved = true;
    }
    if (down & HidNpadButton_AnyRight) {
        App::MoveSel(1);
        moved = true;
    }
    if (down & HidNpadButton_AnyDown) {
        App::SetDockFocus(true);
        App::SetDockSel(0);
        moved = true;
    }
    if (down & HidNpadButton_L) {
        App::MoveSel(-6);
        moved = true;
    }
    if (down & HidNpadButton_R) {
        App::MoveSel(6);
        moved = true;
    }
    if (!moved) {
        u64 dirs = HidNpadButton_AnyLeft | HidNpadButton_AnyRight;
        if (held & dirs) {
            s_rep++;
            if (s_rep > 20 && (s_rep % 6) == 0) {
                if (held & HidNpadButton_AnyLeft)
                    App::MoveSel(-1);
                if (held & HidNpadButton_AnyRight)
                    App::MoveSel(1);
            }
        } else {
            s_rep = 0;
        }
    } else {
        s_rep = 0;
    }
}

} /* namespace WStrip */

