/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Layout.hpp"
#include "../core/App.hpp"
#include "../core/Sfx.hpp"
#include "../core/Icons.hpp"
#include "../core/Qext.hpp"
#include <string.h>
#include <stdio.h>
#include <math.h>

namespace WStrip {

static float s_scroll = 0.0f;
static float s_target = 0.0f;
static float s_maxS = 0.0f;
static unsigned s_rep = 0;
static float s_hlT = 1.0f;

static void TileFallback(const AppRow &r, float x, float y, float w, float h, float rad)
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
    float size = w < h ? w : h;
    Gfx::PushPanel(x, y, w, h, rad, cr, cg, cb, 1.0f);
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

/* TODO: generate the frames at runtime instead of getting them. */
static float s_markT = 0.0f;

static void DrawMarker(float cx, float bandTop, float bandH, float t)
{
    (void)t;
    s_markT += 1.0f / 60.0f;
    if (s_markT >= 4.0f)
        s_markT -= 4.0f;
    int fr = (int)(s_markT * 12.0f);
    if (fr < 0)
        fr = 0;
    if (fr > 47)
        fr = 47;
    int slot = Icons::HudSlot(HUD_MARKER_STRIP);
    if (slot <= 0)
        return;
    float fx = (float)((fr % 14) * 113);
    float fy = (float)((fr / 14) * 51);
    float u0 = fx / 1582.0f, v0 = fy / 204.0f;
    float u1 = (fx + 113.0f) / 1582.0f, v1 = (fy + 51.0f) / 204.0f;
    float mw = 100.0f, mh = 45.0f;
    float mx = cx - mw * 0.5f;
    float my = bandTop + (bandH - mh) * 0.5f;
    Gfx::PushIcon(mx, my, mw, mh, 6.0f, 1.0f, 1.0f, 1.0f, 0.0f, slot, 0.0f, u0, v0, u1, v1);
}

static void DrawWrappedName(const char *name, float tcx, float yTop)
{
    char buf[128];
    unsigned bi = 0;
    while (name[bi] != 0 && bi + 1 < sizeof(buf)) {
        buf[bi] = name[bi];
        bi++;
    }
    buf[bi] = 0;
    const char *words[16];
    int nw = 0;
    for (unsigned i = 0; i < bi && nw < 16;) {
        while (i < bi && buf[i] == 32)
            i++;
        if (i >= bi)
            break;
        words[nw++] = &buf[i];
        while (i < bi && buf[i] != 32)
            i++;
        if (i < bi)
            buf[i++] = 0;
    }
    char line[2][128];
    int nl = 0;
    int wi = 0;
    while (wi < nw && nl < 2) {
        line[nl][0] = 0;
        unsigned len = 0;
        while (wi < nw) {
            const char *w = words[wi];
            unsigned wl = 0;
            while (w[wl] != 0)
                wl++;
            unsigned need = len + (len ? 1 : 0) + wl;
            if (len > 0 && need >= 127)
                break;
            if (len > 0) {
                line[nl][len++] = 32;
            }
            for (unsigned k = 0; k < wl && len + 1 < 127; k++)
                line[nl][len++] = w[k];
            line[nl][len] = 0;
            wi++;
            if (nl == 0 && wi < nw) {
                char trial[128];
                unsigned tl = 0;
                while (line[nl][tl] != 0 && tl + 1 < sizeof(trial)) {
                    trial[tl] = line[nl][tl];
                    tl++;
                }
                trial[tl++] = 32;
                unsigned vl = 0;
                while (words[wi][vl] != 0 && tl + 1 < sizeof(trial))
                    trial[tl++] = words[wi][vl++];
                trial[tl] = 0;
                if (Font::Measure(trial, 30.0f) > 500.0f)
                    break;
            }
        }
        if (line[nl][0] != 0)
            nl++;
        else
            break;
    }
    while (wi < nw && nl > 0) {
        unsigned len = 0;
        while (line[nl - 1][len] != 0)
            len++;
        const char *w = words[wi++];
        if (len > 0 && len + 1 < 127)
            line[nl - 1][len++] = 32;
        while (w[0] != 0 && len + 1 < 127) {
            line[nl - 1][len++] = w[0];
            w++;
        }
        line[nl - 1][len] = 0;
    }
    for (int li = 0; li < nl; li++)
        Font::Centered(line[li], tcx, yTop + (float)li * 36.0f, 30.0f, 500.0f, 0.043f, 0.337f, 0.804f);
}
void Draw(const LayoutNode &nd)
{
    float corner = 24.0f;
    float frame = 6.0f;
    float tile = nd.tile > 0.0f ? nd.tile : 384.0f;
    float gap = nd.gap > 0.0f ? nd.gap : 25.0f;
    float pitch = tile + gap;
    float shx = Layout::ShiftX();
    float shy = App::HomeShiftY();

    int n = App::RowCount();
    int sel = App::Sel();
    /* Focus */
    bool gamesActive = App::Menu() == MENU_HOME && !App::TopFocus() &&
                       !App::BottomFocus() && App::HomeSub() == HOMESUB_GAMES;
    float outer = tile + frame * 2.0f;

    /* Game label 30px accent, 461px wide. */
    if (nd.label && gamesActive && sel >= 0 && sel < n) {
        float lt = s_hlT < 0.0f ? 0.0f : (s_hlT > 1.0f ? 1.0f : s_hlT);
        float tcx = nd.x + shx + (float)sel * pitch - s_scroll + outer * 0.5f;
        DrawWrappedName(App::Row(sel).name, tcx,
                        nd.y + shy + outer + 20.0f * lt + 45.0f);
    }

    float viewW = 1920.0f - nd.x;
    s_maxS = (float)n * pitch + 120.0f - viewW;
    if (s_maxS < 0.0f)
        s_maxS = 0.0f;
    
    if (s_target < 0.0f)
        s_target = 0.0f;
    
    if (s_target > s_maxS)
        s_target = s_maxS;

    if (n > 0 && sel >= 0 && sel < n) {
        float tl = (float)sel * pitch;
        float tr = tl + outer;
        if (tr - s_target > viewW)
            s_target = tr - viewW;
        if (tl - s_target < 0.0f)
            s_target = tl;
    }

    s_scroll += (s_target - s_scroll) * 0.25f;
    if (s_scroll < 0.0f)
        s_scroll = 0.0f;

    int k0 = (int)(s_scroll / pitch) - 1;
    if (k0 < 0)
        k0 = 0;

    int k1 = (int)((s_scroll + 1920.0f) / pitch) + 2;
    if (k1 > n)
        k1 = n;

    if (s_hlT < 1.0f)
        s_hlT += 1.0f / 7.0f;

    if (s_hlT > 1.0f)
        s_hlT = 1.0f;
    
    /* Gamecard animation stuff */
    int selK = (gamesActive && sel >= k0 && sel < k1) ? sel : -1;
    for (int k = k0; k < k1; k++) {
        if (k == selK)
            continue;

        float cx = nd.x + shx + (float)k * pitch - s_scroll + outer * 0.5f;
        const AppRow &r = App::Row(k);
        float x = cx - outer * 0.5f;
        float y = nd.y + shy;

        if (x + outer < -50.0f || x > 1970.0f)
            continue;
        
        if (r.iconSlot > 0) {
            unsigned q = Gfx::PushIcon(x, y, outer, outer, corner, 1.0f, 1.0f, 1.0f, 0.0f, r.iconSlot);
            if (q >= (unsigned)(GFX_ICONQ_MAX / 6))
                TileFallback(r, x, y, outer, outer, corner);
        } else {
            TileFallback(r, x, y, outer, outer, corner);
        }
    }

    if (selK >= 0) {
        float cx = nd.x + shx + (float)selK * pitch - s_scroll + outer * 0.5f;
        const AppRow &r = App::Row(selK);
        float x = cx - outer * 0.5f;
        float y = nd.y + shy;
        float t = s_hlT < 0.0f ? 0.0f : (s_hlT > 1.0f ? 1.0f : s_hlT);

        /* More gamecard stuff. */
        float grow = 84.0f * t;
        float gx = x, gy = y - 84.0f * t, gw = outer, gh = outer + grow;
        Gfx::PushSelectRing(gx - 9.0f, gy - 9.0f, gw + 18.0f, gh + 18.0f, corner + 9.0f, 12.0f);

        /* Rounded top and square bottom. */
        if (r.iconSlot > 0) {
            unsigned q = Gfx::PushIcon(gx, gy, outer, outer, corner, 1.0f, 1.0f, 1.0f, 0.0f, r.iconSlot, 0.0f);
            if (q >= (unsigned)(GFX_ICONQ_MAX / 6))
                TileFallback(r, gx, gy, outer, outer, corner);
        } else {
            TileFallback(r, gx, gy, outer, outer, corner);
        }

        if (grow > 0.5f) {
            Gfx::PushPanel(gx, gy + outer, gw, grow, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            DrawMarker(cx, gy + outer, grow, t);
        }
    }
}

/* Draw folders/VGC (TBD: What do do with VGC, maybe file manager?) */
void DrawSubs()
{
    float shx = Layout::ShiftX();
    float shy = App::HomeShiftY();
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);

    /* Folders screen. TODO: Fix this */
    {
        float y0 = 293.0f + 830.0f + shy;
        if (y0 > -400.0f && y0 < 1100.0f) {
            Font::Centered("Folders", 960.0f + shx, y0 - 60.0f, 44.0f,
                           800.0f, inkR, inkG, inkB);
            float tw = 300.0f, gap = 40.0f;
            float x0 = 960.0f - (tw * 3.0f + gap * 2.0f) * 0.5f + shx;
            int fslot = Icons::HudSlot(HUD_FOLDER);
            for (int i = 0; i < 3; i++) {
                char nm[24];
                snprintf(nm, sizeof(nm), "Folder %d", i + 1);
                float fx = x0 + i * (tw + gap);
                float fr, fg, fb;
                Theme::Color("panel", &fr, &fg, &fb);
                Gfx::PushPanel(fx, y0, tw, tw, 24.0f, fr, fg, fb, 1.0f);
                if (fslot > 0) {
                    Gfx::PushIcon(fx + 40.0f, y0 + 20.0f, 220.0f, 220.0f, 16.0f, 1.0f, 1.0f, 1.0f, 0.0f, fslot);
                }
                float nw = Font::Measure(nm, 36.0f);
                Font::Draw(nm, fx + (tw - nw) * 0.5f, y0 + tw + 48.0f,
                           36.0f, dimR, dimG, dimB);
            }
        }
    }

    /* VGC grid. */
    {
        float y0 = 293.0f - 935.0f + shy;
        if (y0 > -700.0f && y0 < 1100.0f) {
            Font::Centered("Virtual Game Cards", 960.0f + shx, y0 - 60.0f,
                           44.0f, 900.0f, inkR, inkG, inkB);
            int n = App::RowCount();
            int sel = App::Sel();
            float tw = 180.0f, gap = 20.0f, pitch = tw + gap;
            int cols = 6;
            float x0 = 960.0f - (cols * pitch - gap) * 0.5f + shx;
            for (int k = 0; k < n && k < 18; k++) {
                int cx = k % cols, cy = k / cols;
                float x = x0 + cx * pitch;
                float y = y0 + cy * (tw + 44.0f);
                const AppRow &r = App::Row(k);
                bool isSel = (k == sel);
                if (isSel)
                    Gfx::PushSelectRing(x - 8.0f, y - 8.0f, tw + 16.0f, tw + 16.0f, 20.0f, 8.0f);
                if (r.iconSlot > 0) {
                    unsigned q = Gfx::PushIcon(x, y, tw, tw, 12.0f, 1.0f, 1.0f, 1.0f, 0.0f, r.iconSlot);
                    if (q >= (unsigned)(GFX_ICONQ_MAX / 6))
                        TileFallback(r, x, y, tw, tw, 12.0f);
                } else {
                    TileFallback(r, x, y, tw, tw, 12.0f);
                }
            }
        }
    }
}

static bool StepSel(int delta)
{
    int n = App::RowCount();
    if (n <= 0)
        return false;

    int sel = App::Sel();
    sel += delta;

    /* No wraparound. */
    if (sel < 0)
        sel = 0;
    else if (sel >= n)
        sel = n - 1;

    /* Ensure we don't restart animation every time. */
    if (sel != App::Sel()) {
        App::SetSel(sel);
        s_hlT = 0.0f;
        Sfx::Play(Sfx::Hover);
        App::SetTopFocus(false);
        return true;
    }

    App::SetTopFocus(false);
    return false;
}

void Input(const LayoutNode &nd, u64 down, u64 held)
{
    (void)nd;
    if (App::TopFocus() || App::Menu() != MENU_HOME)
        return;

    /* Dont input during a transition. */
    if (App::HomeShiftBusy())
        return;
    
    /* Folders / VGC submenus. */
    if (App::HomeSub() == HOMESUB_FOLDERS) {
        if (down & HidNpadButton_A) {
            App::SetHomeSub(HOMESUB_GAMES);
            Sfx::Play(Sfx::Click);
        } else if (down & (HidNpadButton_B | HidNpadButton_AnyUp)) {
            App::SetHomeSub(HOMESUB_GAMES);
            Sfx::Play(Sfx::Back);
        }
        return;
    }

    if (App::HomeSub() == HOMESUB_VGC) {
        if (down & HidNpadButton_AnyLeft) {
            StepSel(-1);
            return;
        }
        if (down & HidNpadButton_AnyRight) {
            StepSel(1);
            return;
        }
        if (down & (HidNpadButton_B | HidNpadButton_AnyUp)) {
            App::SetHomeSub(HOMESUB_GAMES);
            Sfx::Play(Sfx::Back);
        }
        return;
    }

    if (App::BottomFocus())
        return;
    
    bool moved = false;
    if (down & HidNpadButton_AnyLeft) {
        StepSel(-1);
        moved = true;
    }

    if (down & HidNpadButton_AnyRight) {
        StepSel(1);
        moved = true;
    }

    if (down & HidNpadButton_AnyUp) {
        App::SetTopSel(App::Menu());
        App::SetTopFocus(true);
        Sfx::Play(Sfx::Hover);
        moved = true;
    }
    
    if (down & HidNpadButton_AnyDown) {
        App::SetBottomSel(1);
        App::SetBottomFocus(true);
        Sfx::Play(Sfx::Hover);
        moved = true;
    }
    
    if (!moved) {
        u64 dirs = HidNpadButton_AnyLeft | HidNpadButton_AnyRight;
        if (held & dirs) {
            s_rep++;
            if (s_rep > 20 && (s_rep % 6) == 0) {
                if (held & HidNpadButton_AnyLeft)
                    StepSel(-1);
                if (held & HidNpadButton_AnyRight)
                    StepSel(1);
            }
        } else {
            s_rep = 0;
        }
    } else {
        s_rep = 0;
    }
}

} /* namespace WStrip */
