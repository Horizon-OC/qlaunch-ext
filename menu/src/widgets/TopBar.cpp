/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Clock.hpp"

namespace WTopBar {

void Draw(const LayoutNode &nd)
{
    (void)nd;
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);

    /* Avatar placeholder. */
    float avR, avG, avB;
    Theme::Color("avatar", &avR, &avG, &avB);
    Gfx::PushPanel(14.0f, 26.0f, 92.0f, 92.0f, 46.0f, 1.0f, 1.0f, 1.0f, 1.0f);
    Gfx::PushPanel(20.0f, 32.0f, 80.0f, 80.0f, 40.0f, avR, avG, avB, 1.0f);

    /* Clock, right aligned. */
    float cw = Font::Measure(Clock::Str(), 36.0f);
    Font::Draw(Clock::Str(), 1700.0f - cw, 92.0f, 36.0f, inkR, inkG, inkB);

    /* Wifi glyph. */
    char wg[4] = { 0 };
    GlyphUTF8(GLYPH_WIFI, wg);
    float ww = Font::Measure(wg, 40.0f);
    Font::Draw(wg, 1724.0f - ww * 0.5f, 92.0f, 40.0f, inkR, inkG, inkB);

    /* Battery: outline + level fill + nub. */
    float baR, baG, baB;
    Theme::Color("battery", &baR, &baG, &baB);
    Gfx::PushPanel(1764.0f, 58.0f, 64.0f, 32.0f, 8.0f, baR, baG, baB, 1.0f);
    float pct = (float)Clock::Batt() / 100.0f;
    float fr = 0.30f, fg = 0.78f, fb = 0.35f;
    if (Clock::Batt() < 20) {
        fr = 0.95f;
        fg = 0.30f;
        fb = 0.30f;
    }
    if (pct > 0.0f)
        Gfx::PushPanel(1768.0f, 62.0f, 56.0f * pct, 24.0f, 5.0f,
                       fr, fg, fb, 1.0f);
    Gfx::PushPanel(1828.0f, 66.0f, 8.0f, 16.0f, 3.0f, baR, baG, baB, 1.0f);
}

} /* namespace WTopBar */

