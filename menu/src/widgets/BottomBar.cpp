/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Icons.hpp"
#include "../core/Layout.hpp"
#include "../core/App.hpp"
#include "../core/Sfx.hpp"
#include "../core/PadIcon.hpp"
#include "../core/Input.hpp"
#include "../core/Tabs.hpp"

namespace WBottomBar {

    namespace {

        /// @brief Bottom-bar transient state (key repeat only).
        struct BottomState {
            InputRepeat repeat;
        };

        static BottomState s_state = { { 0 } };

        /// @brief Resolve a HUD icon slot.
        /// @param which HUD_* id.
        /// @return Icon slot.
        int IconSlot(int which)
        {
            return Icons::HudSlot(which);
        }

    } // namespace

    /// @brief Draw the bottom bar (pad icon + home shortcuts).
    void Draw()
    {
        float mbR, mbG, mbB;
        Theme::Color("menubar", &mbR, &mbG, &mbB);
        Gfx::PushPanel(0.0f, 924.0f, 1920.0f, 156.0f, 0.0f, mbR, mbG, mbB, 1.0f);

        float inkR, inkG, inkB;
        Theme::Color("ink", &inkR, &inkG, &inkB);

        /* Controller symbol. TODO: add better controller detection. */
        unsigned cp = PadIcon::Glyph();
        float cr, cg, cb;
        PadIcon::Color(&cr, &cg, &cb);
        char gb[4] = { 0 };
        GlyphUTF8(cp, gb);
        float gw = Font::Measure(gb, 72.0f);
        Font::Draw(gb, 76.0f + (100.0f - gw) * 0.5f, 957.0f + 62.0f, 72.0f, cr, cg, cb);

        int homeIdx = Tabs::FindById("home");

        /* Only on the home tab. */
        if (App::Menu() == homeIdx) {
            float size = 72.0f, gap = 24.0f;
            /* M-FOLDER 1614, M-VGC 1710, sleep 1806. */
            float x0 = 1578.0f;
            float y = 958.0f;
            float bR = 1.0f, bG = 1.0f, bB = 1.0f;

            if (Theme::IsDark()) {
                bR = 0.16f;
                bG = 0.16f;
                bB = 0.18f;
            }

            for (int i = 0; i < 3; i++) {
                bool focused = App::BottomFocus() && App::BottomSel() == i;
                float cx = x0 + (float)i * (size + gap);
                float bd = size + 16.0f;
                Gfx::PushPanel(cx - 8.0f, y - 8.0f, bd, bd, bd * 0.5f, bR, bG, bB, 1.0f);

                if (i == 2) {
                    char pw[4] = { 0 };
                    GlyphUTF8(GLYPH_POWER, pw);
                    float pwW = Font::Measure(pw, 52.0f);
                    float pr = inkR, pg = inkG, pb = inkB;

                    if (Theme::IsDark()) {
                        pr = pg = pb = 1.0f;
                    }

                    Font::Draw(pw, cx + (size - pwW) * 0.5f, y + size * 0.5f + 18.0f, 52.0f, pr, pg, pb);
                } else {
                    int slot = (i == 0) ? IconSlot(HUD_FOLDER) : IconSlot(HUD_VGCCARD);

                    if (Gfx::TexValid(slot)) {
                        Gfx::PushIcon(cx, y + 8.0f, size, size, 16.0f, 16.0f, 16.0f, 16.0f, 1.0f, 1.0f, 1.0f, 0.0f, slot);
                    }
                }

                if (focused) {
                    Gfx::PushSelectRing(cx - 9.0f, y - 9.0f, bd + 2.0f, bd + 2.0f, (bd + 2.0f) * 0.5f, 10.0f);
                }
            }
        }
    }

    /// @brief Handle bottom-bar navigation input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    void Input(u64 down, u64 held)
    {
        int homeIdx = Tabs::FindById("home");

        if (!App::BottomFocus() || App::Menu() != homeIdx) {
            return;
        }

        if (App::HomeShiftBusy()) {
            return;
        }

        s_state.repeat.Poll(down, held, HidNpadButton_AnyLeft | HidNpadButton_AnyRight, DefaultRepeat());

        if (down & HidNpadButton_AnyLeft) {
            int s = App::BottomSel();

            if (s > 0) {
                App::SetBottomSel(s - 1);
                Sfx::Play(Sfx::Hover);
            }
        }

        if (down & HidNpadButton_AnyRight) {
            int s = App::BottomSel();

            if (s < 2) {
                App::SetBottomSel(s + 1);
                Sfx::Play(Sfx::Hover);
            }
        }

        if (down & HidNpadButton_AnyUp) {
            App::SetBottomFocus(false);
            Sfx::Play(Sfx::Hover);
        }

        if (down & HidNpadButton_B) {
            App::SetBottomFocus(false);
            Sfx::Play(Sfx::Back);
        }
    }

} /* namespace WBottomBar */
