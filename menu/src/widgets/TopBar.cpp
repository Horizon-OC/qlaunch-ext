/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Clock.hpp"
#include "../core/Icons.hpp"
#include "../core/Layout.hpp"
#include "../core/App.hpp"
#include "../core/Avatars.hpp"
#include <stdio.h>

namespace WTopBar {

static int TopArt(int m, bool active)
{
    if (active)
        return Icons::HudSlot(HUD_TOPDES_HOME + m);
    return Icons::HudSlot(HUD_TOPSEL_HOME + m);
}

static unsigned MenuGlyph(int m)
{
    switch (m) {
    case MENU_HOME: return GLYPH_HOME;
    case MENU_CONNECT: return GLYPH_CHAT;
    case MENU_ESHOP: return GLYPH_ESHOP;
    case MENU_ALBUM: return GLYPH_ALBUM;
    case MENU_SETTINGS: return GLYPH_GEAR;
    default: return 0;
    }
}

void Draw(const LayoutNode &nd)
{
    (void)nd;
    float mbR, mbG, mbB;
    Theme::Color("menubar", &mbR, &mbG, &mbB);
    Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 201.0f, 0.0f, mbR, mbG, mbB, 1.0f);

    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);

    /* TODO: Fix weird pink background */
    float avR, avG, avB;
    Theme::Color("avatar", &avR, &avG, &avB);
    int nusers = Avatars::Count();

    if (nusers > 2)
        nusers = 2;
    
    for (int i = 0; i < nusers; i++) {
        float ax = 75.0f + (float)i * 103.0f;
        Gfx::PushPanel(ax, 66.0f, 92.0f, 92.0f, 46.0f, dimR, dimG, dimB,
                       1.0f);
        Gfx::PushPanel(ax + 5.0f, 71.0f, 82.0f, 82.0f, 41.0f, avR, avG,
                       avB, 1.0f);
        int aslot = Avatars::Slot(i);
        if (aslot <= 0)
            aslot = Icons::HudSlot(HUD_USER);
        if (aslot > 0) {
            Gfx::PushIcon(ax + 9.0f, 75.0f, 74.0f, 74.0f, 30.0f, 1.0f, 1.0f, 1.0f, 0.0f, aslot);
        }
    }

    float bw = 98.0f, gap = 5.0f;
    float x0 = (1920.0f - (bw * 5.0f + gap * 4.0f)) * 0.5f;
    float by = 67.0f;
    int cur = App::Menu();

    for (int i = 0; i < 5; i++) {
        bool active = (i == cur);
        bool focused = App::TopFocus() && App::TopSel() == i;

        float scale = active ? 1.2f : (focused ? 1.1f : 1.0f);
        float size = bw * scale;
        float cx = x0 + bw * (float)i + gap * (float)i + bw * 0.5f;
        float x = cx - size * 0.5f;
        float y = by + (bw - size) * 0.5f;
        int aslot = TopArt(i, active);

        if (aslot > 0) {
            float cr, cg, cb;

            if (active) {
                /* Filled art takes the per-button theme color. */
                Theme::MenuAccent(i, &cr, &cg, &cb);
            } else if (Theme::IsDark()) {
                cr = cg = cb = 1.0f;
            } else {
                cr = inkR;
                cg = inkG;
                cb = inkB;
            }

            Gfx::PushIcon(x, y, size, size, 0.0f, cr, cg, cb, 0.0f, aslot);

            if (focused && !active)
                Gfx::PushSelectRing(x - 7.0f, y - 7.0f, size + 14.0f, size + 14.0f, (size + 14.0f) * 0.5f, 9.0f);
        } else {
            /* Art missing: font glyph fallback. */
            unsigned cp = MenuGlyph(i);
            if (!cp)
                continue;
            
            float cr, cg, cb;

            if (active) {
                Theme::MenuAccent(i, &cr, &cg, &cb);
            } else {
                cr = inkR;
                cg = inkG;
                cb = inkB;
            }

            char gb[4] = { 0 };
            GlyphUTF8(cp, gb);
            float gs = size * 0.62f;
            float gw = Font::Measure(gb, gs);
            Font::Draw(gb, x + (size - gw) * 0.5f,
                       y + size * 0.5f + gs * 0.35f, gs, cr, cg, cb);
        }
    }

    /* Right cluster. TODO: Fix alignment */
    {
        /* Clock */
        char tb[16];
        snprintf(tb, sizeof(tb), "%s", Clock::TimeStr());
        char ab[8];
        snprintf(ab, sizeof(ab), "%s", Clock::AmPm());
        float tw = Font::Measure(tb, 40.0f);
        float aw = Font::Measure(ab, 26.0f);
        float tx = 1694.0f - tw - 8.0f - aw;
        Font::Draw(tb, tx, 130.0f, 40.0f, inkR, inkG, inkB);
        Font::Draw(ab, tx + tw + 8.0f, 130.0f, 26.0f, inkR, inkG, inkB);
    }
    {
        int wslot = Icons::HudSlot(HUD_WIFI);
        if (wslot > 0) {
            Gfx::PushIcon(1717.0f, 88.0f, 51.0f, 51.0f, 0.0f, mbR, mbG, mbB, 0.0f, wslot);
        } else {
            char wg[4] = { 0 };
            GlyphUTF8(GLYPH_WIFI, wg);
            Font::Draw(wg, 1717.0f, 130.0f, 44.0f, inkR, inkG, inkB);
        }
    }
    /* Charging symbol. Probably needs adjustment */
    {
        int bslot = Icons::HudSlot(HUD_BATTERY);
        if (bslot > 0) {
            Gfx::PushIcon(1790.0f, 81.0f, 64.0f, 64.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, bslot);
        } else {
            unsigned batt = Clock::Batt();
            if (batt > 100)
                batt = 100;
            float pct = (float)batt / 100.0f;
            Gfx::PushPanel(1790.0f, 81.0f, 64.0f, 36.0f, 8.0f, inkR, inkG,
                           inkB, 1.0f);
            Gfx::PushPanel(1795.0f, 86.0f, 54.0f, 26.0f, 5.0f, mbR, mbG,
                           mbB, 1.0f);
            if (pct > 0.0f)
                Gfx::PushPanel(1795.0f, 86.0f, 54.0f * pct, 26.0f, 5.0f,
                               inkR, inkG, inkB, 1.0f);
            Gfx::PushPanel(1854.0f, 90.0f, 8.0f, 18.0f, 3.0f, inkR, inkG,
                           inkB, 1.0f);
        }
    }
}

void Input(u64 down, u64 held)
{
    (void)held;
    if (!App::TopFocus())
        return;
    int s = App::TopSel();
    if (down & HidNpadButton_AnyLeft) {
        if (s > 0)
            App::SetTopSel(s - 1);
    }
    if (down & HidNpadButton_AnyRight) {
        if (s < 4)
            App::SetTopSel(s + 1);
    }

    if (down & (HidNpadButton_AnyDown | HidNpadButton_B))
        App::SetTopFocus(false);
}

} /* namespace WTopBar */