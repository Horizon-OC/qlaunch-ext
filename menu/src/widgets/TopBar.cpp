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
#include "../core/Sfx.hpp"
#include "../core/Avatars.hpp"
#include "../core/Tabs.hpp"
#include "../core/Input.hpp"
#include <stdio.h>

namespace WTopBar {

    namespace {

        /// @brief Per-widget transient state (recolor anim + key repeat).
        struct TopBarState {
            float recolorT;
            int lastFocus;
            InputRepeat repeat;
        };

        static TopBarState s_state = { 0.0f, -2, { 0 } };

        /// @brief Resolve the art slot for a tab.
        /// @param tab Tabs registry index.
        /// @param active True when the tab is current.
        /// @return HUD icon slot, or <= 0 when missing.
        int TopArt(int tab, bool active)
        {
            const MenuTab *desc = Tabs::Get(tab);

            if (desc && desc->hudSel > 0 && desc->hudDesel > 0) {
                return active ? desc->hudDesel : desc->hudSel;
            }

            if (active) {
                return Icons::HudSlot(HUD_TOPDES_HOME + tab);
            }

            return Icons::HudSlot(HUD_TOPSEL_HOME + tab);
        }

        /// @brief Fallback glyph for a tab.
        /// @param tab Tabs registry index.
        /// @return Unicode codepoint, or 0.
        unsigned TabGlyph(int tab)
        {
            const MenuTab *desc = Tabs::Get(tab);

            if (desc && desc->glyph) {
                return desc->glyph;
            }

            switch (tab) {
            case MENU_HOME:
                return GLYPH_HOME;
            case MENU_CONNECT:
                return GLYPH_CHAT;
            case MENU_ESHOP:
                return GLYPH_ESHOP;
            case MENU_ALBUM:
                return GLYPH_ALBUM;
            case MENU_SETTINGS:
                return GLYPH_GEAR;
            default:
                return 0;
            }
        }

        /// @brief Recolor a section with the S2 gradient.
        /// @param t 0-1 animation phase.
        /// @param r Red output.
        /// @param g Green output.
        /// @param b Blue output.
        void Recolor(float t, float *r, float *g, float *b)
        {
            static const float pal[4][3] = {
                { 0.537f, 0.584f, 0.945f },
                { 0.384f, 0.588f, 0.949f },
                { 0.863f, 0.722f, 0.875f },
                { 1.0f, 1.0f, 1.0f },
            };

            float seg = t * 4.0f;
            int si = (int)seg;
            int i = si & 3;
            float f = seg - (float)si;
            const float *A = pal[i];
            const float *B = pal[(i + 1) & 3];
            *r = A[0] + (B[0] - A[0]) * f;
            *g = A[1] + (B[1] - A[1]) * f;
            *b = A[2] + (B[2] - A[2]) * f;
        }

    } // namespace

    /// @brief Draw the top bar (avatars, tab icons, clock, status).
    /// @param nd Layout node (currently unused, geometry is fixed).
    void Draw(const LayoutNode &nd)
    {
        (void)nd;

        int focusId = App::TopFocus() ? App::TopSel() : -1;

        if (focusId != s_state.lastFocus) {
            s_state.lastFocus = focusId;
            s_state.recolorT = 0.0f;
        } else {
            s_state.recolorT += 0.5f / 60.0f;

            if (s_state.recolorT >= 1.0f) {
                s_state.recolorT -= 1.0f;
            }
        }

        float mbR, mbG, mbB;
        Theme::Color("menubar", &mbR, &mbG, &mbB);
        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 201.0f, 0.0f, mbR, mbG, mbB, 1.0f);

        float inkR, inkG, inkB;
        Theme::Color("ink", &inkR, &inkG, &inkB);
        float dimR, dimG, dimB;
        Theme::Color("dim", &dimR, &dimG, &dimB);

        int nusers = Avatars::Count();

        if (nusers > 2) {
            nusers = 2;
        }

        for (int i = 0; i < nusers; i++) {
            float ax = 75.0f + (float)i * 103.0f;
            Gfx::PushPanel(ax, 66.0f, 92.0f, 92.0f, 46.0f, dimR, dimG, dimB, 1.0f);

            int aslot = Avatars::Slot(i);

            if (aslot <= 0) {
                aslot = Icons::HudSlot(HUD_USER);
            }

            if (aslot > 0) {
                Gfx::PushIcon(ax + 9.0f, 75.0f, 74.0f, 74.0f, 30.0f, 1.0f, 1.0f, 1.0f, 0.0f, aslot);
            }
        }

        float bw = 98.0f, gap = 5.0f;
        float aw = bw * 1.4f;
        int count = Tabs::Count();

        if (count <= 0) {
            count = MENU_COUNT;
        }

        float ttot = bw * (float)(count - 1) + aw + gap * (float)(count - 1);
        float tx = (1920.0f - ttot) * 0.5f;
        float by = 67.0f;
        int cur = App::Menu();

        for (int i = 0; i < count; i++) {
            bool active = (i == cur);
            bool focused = App::TopFocus() && App::TopSel() == i;

            float sw = active ? aw : bw;
            float size = active ? sw : bw * (focused ? 1.2f : 1.0f);
            float cx = tx + sw * 0.5f;
            float x = cx - size * 0.5f;
            float y = by + (bw - size) * 0.5f;
            tx += sw + gap;

            int aslot = TopArt(i, active);

            if (aslot > 0) {
                float cr, cg, cb;

                if (focused) {
                    Recolor(s_state.recolorT, &cr, &cg, &cb);
                } else if (active) {
                    Theme::MenuAccent(i, &cr, &cg, &cb);
                } else if (Theme::IsDark()) {
                    cr = cg = cb = 1.0f;
                } else {
                    cr = inkR;
                    cg = inkG;
                    cb = inkB;
                }

                Gfx::PushIcon(x, y, size, size, 0.0f, cr, cg, cb, 0.0f, aslot);
            } else {
                unsigned cp = TabGlyph(i);

                if (!cp) {
                    continue;
                }

                float cr, cg, cb;

                if (focused) {
                    Recolor(s_state.recolorT, &cr, &cg, &cb);
                } else if (active) {
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
                Font::Draw(gb, x + (size - gw) * 0.5f, y + size * 0.5f + gs * 0.35f, gs, cr, cg, cb);
            }
        }

        /* Right cluster. TODO: Fix alignment. */
        {
            char tb[16];
            snprintf(tb, sizeof(tb), "%s", Clock::TimeStr());
            char ab[8];
            snprintf(ab, sizeof(ab), "%s", Clock::AmPm());
            float tw = Font::Measure(tb, 40.0f);
            float aw2 = Font::Measure(ab, 26.0f);
            float tx2 = 1694.0f - tw - 8.0f - aw2;
            Font::Draw(tb, tx2, 130.0f, 40.0f, inkR, inkG, inkB);
            Font::Draw(ab, tx2 + tw + 8.0f, 130.0f, 26.0f, inkR, inkG, inkB);
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
        {
            int bslot = Icons::HudSlot(HUD_BATTERY);

            if (bslot > 0) {
                Gfx::PushIcon(1790.0f, 81.0f, 64.0f, 64.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, bslot);
            } else {
                unsigned batt = Clock::Batt();

                if (batt > 100) {
                    batt = 100;
                }

                float pct = (float)batt / 100.0f;
                Gfx::PushPanel(1790.0f, 81.0f, 64.0f, 36.0f, 8.0f, inkR, inkG, inkB, 1.0f);
                Gfx::PushPanel(1795.0f, 86.0f, 54.0f, 26.0f, 5.0f, mbR, mbG, mbB, 1.0f);

                if (pct > 0.0f) {
                    Gfx::PushPanel(1795.0f, 86.0f, 54.0f * pct, 26.0f, 5.0f, inkR, inkG, inkB, 1.0f);
                }

                Gfx::PushPanel(1854.0f, 90.0f, 8.0f, 18.0f, 3.0f, inkR, inkG, inkB, 1.0f);
            }
        }
    }

    /// @brief Handle top-bar navigation input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    void Input(u64 down, u64 held)
    {
        if (!App::TopFocus()) {
            return;
        }

        int count = Tabs::Count();

        if (count <= 0) {
            return;
        }

        s_state.repeat.Poll(down, held, HidNpadButton_AnyLeft | HidNpadButton_AnyRight, DefaultRepeat());

        int s = App::TopSel();

        if (down & HidNpadButton_AnyLeft) {
            if (s > 0) {
                App::SetTopSel(s - 1);
                Sfx::Play(Sfx::Hover);
            }
        }

        if (down & HidNpadButton_AnyRight) {
            if (s < count - 1) {
                App::SetTopSel(s + 1);
                Sfx::Play(Sfx::Hover);
            }
        }

        if (down & HidNpadButton_AnyDown) {
            App::SetTopFocus(false);
            Sfx::Play(Sfx::Hover);
        }

        if (down & HidNpadButton_B) {
            App::SetTopFocus(false);
            Sfx::Play(Sfx::Back);
        }
    }

} /* namespace WTopBar */
