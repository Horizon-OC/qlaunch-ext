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
#include "../core/Tabs.hpp"
#include <string.h>

/* TODO: Do we remove this or keep it somewhere? (alternate mode?)*/
namespace WHints {

    namespace {

        /// @brief Maximum hints drawn from a layout node or fallback.
        constexpr int kMaxHints = 3;

        /// @brief One resolved footer hint.
        struct ResolvedHint {
            const char *button;
            const char *label;
        };

        /// @brief Map a button name to its glyph fallback.
        /// @param b Button name.
        /// @return Glyph codepoint, or 0.
        unsigned ButtonGlyph(const char *b)
        {
            if (strcmp(b, "A") == 0) {
                return GLYPH_A;
            }

            if (strcmp(b, "B") == 0) {
                return GLYPH_B;
            }

            if (strcmp(b, "X") == 0) {
                return GLYPH_X;
            }

            if (strcmp(b, "Y") == 0) {
                return GLYPH_Y;
            }

            if (strcmp(b, "+") == 0) {
                return GLYPH_PLUS;
            }

            if (strcmp(b, "-") == 0) {
                return GLYPH_MINUS;
            }

            return 0;
        }

        /// @brief Pick fallback hints from app focus state.
        /// @param out Output buffer.
        /// @return Number of hints written.
        int FallbackHints(ResolvedHint *out)
        {
            int hn = 0;
            int homeIdx = Tabs::FindById("home");
            int settingsIdx = Tabs::FindById("settings");

            if (App::TopFocus()) {
                out[hn++] = { "A", "OK" };
                out[hn++] = { "B", "Back" };
            } else if (App::Menu() == settingsIdx) {
                out[hn++] = { "A", "OK" };
                out[hn++] = { "B", "Back" };
            } else if (App::Menu() == homeIdx && App::BottomFocus()) {
                out[hn++] = { "A", "OK" };
                out[hn++] = { "B", "Back" };
            } else if (App::Menu() == homeIdx && App::HomeSub() != HOMESUB_GAMES) {
                out[hn++] = { "B", "Back" };
            } else {
                out[hn++] = { "+", "Options" };
                out[hn++] = { "A", "Start" };
            }

            return hn;
        }

    } // namespace

    /// @brief Draw footer hints from the layout node, else fallback state hints.
    /// @param nd Layout node carrying optional hint items.
    void Draw(const LayoutNode &nd)
    {
        float inkR, inkG, inkB;
        Theme::Color("ink", &inkR, &inkG, &inkB);
        float dimR, dimG, dimB;
        Theme::Color("dim", &dimR, &dimG, &dimB);

        /* Footer divider. */
        Gfx::PushPanel(60.0f, 940.0f, 1680.0f, 3.0f, 0.0f, dimR, dimG, dimB, 1.0f);

        ResolvedHint hs[kMaxHints];
        int hn = 0;

        if (nd.hintCount > 0) {
            for (int i = 0; i < nd.hintCount && hn < kMaxHints; i++) {
                hs[hn].button = nd.hints[i].button;
                hs[hn].label = nd.hints[i].label;
                hn++;
            }
        } else {
            hn = FallbackHints(hs);
        }

        float y = 978.0f;
        float x = 1740.0f;

        for (int i = hn - 1; i >= 0; i--) {
            const char *b = hs[i].button;
            const char *l = hs[i].label;

            float lx = Font::Measure(l, 36.0f);
            Font::Draw(l, x - lx, y, 36.0f, dimR, dimG, dimB);

            x -= lx + 14.0f;
            int aslot = -1;

            if (strcmp(b, "A") == 0) {
                aslot = Icons::HudSlot(HUD_BTN_A);
            } else if (strcmp(b, "+") == 0) {
                aslot = Icons::HudSlot(HUD_BTN_PLUS);
            }

            if (Gfx::TexValid(aslot)) {
                float mbR, mbG, mbB;
                Theme::Color("menubar", &mbR, &mbG, &mbB);
                Gfx::PushIcon(x - 40.0f, y - 34.0f, 40.0f, 40.0f, 0.0f, 0.0f, 0.0f, 0.0f, mbR, mbG, mbB, 0.0f, aslot);
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

} /* namespace WHints */
