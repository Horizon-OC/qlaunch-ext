/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/App.hpp"
#include "../core/Icons.hpp"
#include "../core/Layout.hpp"
#include <string.h>

/* TODO: Do we remove this or keep it somewhere? (alternate mode?)*/
namespace WHints {

static unsigned ButtonGlyph(const char *b)
{
    if (strcmp(b, "A") == 0)
        return GLYPH_A;
    if (strcmp(b, "B") == 0)
        return GLYPH_B;
    if (strcmp(b, "X") == 0)
        return GLYPH_X;
    if (strcmp(b, "Y") == 0)
        return GLYPH_Y;
    if (strcmp(b, "+") == 0)
        return GLYPH_PLUS;
    if (strcmp(b, "-") == 0)
        return GLYPH_MINUS;
    return 0;
}

void Draw(const LayoutNode &nd)
{
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);

    /* Footer divider */
    Gfx::PushPanel(60.0f, 940.0f, 1680.0f, 3.0f, 0.0f, dimR, dimG, dimB,
                   1.0f);

    {
        struct HH { const char *b; const char *l; };
        HH hs[3];
        int hn = 0;
        if (App::TopFocus()) {
            hs[hn].b = "A";
            hs[hn].l = "OK";
            hn++;
            hs[hn].b = "B";
            hs[hn].l = "Back";
            hn++;
        } else if (App::Menu() == MENU_SETTINGS) {
            hs[hn].b = "A";
            hs[hn].l = "OK";
            hn++;
            hs[hn].b = "B";
            hs[hn].l = "Back";
            hn++;
        } else if (App::Menu() == MENU_HOME && App::BottomFocus()) {
            hs[hn].b = "A";
            hs[hn].l = "OK";
            hn++;
            hs[hn].b = "B";
            hs[hn].l = "Back";
            hn++;
        } else if (App::Menu() == MENU_HOME && App::HomeSub() != HOMESUB_GAMES) {
            hs[hn].b = "B";
            hs[hn].l = "Back";
            hn++;
        } else {
            hs[hn].b = "+";
            hs[hn].l = "Options";
            hn++;
            hs[hn].b = "A";
            hs[hn].l = "Start";
            hn++;
        }

        float y = 978.0f;
        float x = 1740.0f;
        for (int i = hn - 1; i >= 0; i--) {
            const char *b = hs[i].b;
            const char *l = hs[i].l;

            float lx = Font::Measure(l, 36.0f);
            Font::Draw(l, x - lx, y, 36.0f, dimR, dimG, dimB);
            
            x -= lx + 14.0f;
            int aslot = -1;

            if (strcmp(b, "A") == 0)
                aslot = Icons::HudSlot(HUD_BTN_A);
            else if (strcmp(b, "+") == 0)
                aslot = Icons::HudSlot(HUD_BTN_PLUS);
            
            if (aslot > 0) {
                /* Button art. */
                float mbR, mbG, mbB;
                Theme::Color("menubar", &mbR, &mbG, &mbB);
                Gfx::PushIcon(x - 40.0f, y - 34.0f, 40.0f, 40.0f, 0.0f, mbR, mbG, mbB, 0.0f, aslot);
                x -= 40.0f + 40.0f;
            } else {
                unsigned cp = ButtonGlyph(b);

                if (cp) {
                    char gb[4] = { 0 };
                    GlyphUTF8(cp, gb);
                    float gw = Font::Measure(gb, 38.0f);
                    Font::Draw(gb, x - gw, y - 2.0f, 38.0f, dimR, dimG, dimB);
                    x -= gw + 40.0f;
                } else {
                    float bw = Font::Measure(b, 36.0f);
                    Font::Draw(b, x - bw, y, 36.0f, dimR, dimG, dimB);
                    x -= bw + 40.0f;
                }
            }
        }
    }
}

} /* namespace WHints */

