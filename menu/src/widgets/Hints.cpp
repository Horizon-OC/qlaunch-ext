/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include <string.h>

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

    /* Joy-con pair deco, bottom-left. */
    Gfx::PushPanel(36.0f, 946.0f, 34.0f, 68.0f, 12.0f, dimR, dimG, dimB, 1.0f);
    Gfx::PushPanel(74.0f, 946.0f, 34.0f, 68.0f, 12.0f, dimR, dimG, dimB, 1.0f);
    Gfx::PushPanel(44.0f, 958.0f, 8.0f, 8.0f, 4.0f, inkR, inkG, inkB, 1.0f);
    Gfx::PushPanel(82.0f, 958.0f, 8.0f, 8.0f, 4.0f, inkR, inkG, inkB, 1.0f);

    /* Hint rows, bottom-right. */
    float y = 948.0f;
    for (int i = 0; i < nd.hintCount; i++) {
        const HintItem &h = nd.hints[i];
        float lx = Font::Measure(h.label, 32.0f);
        Font::Draw(h.label, 1850.0f - lx, y + 32.0f, 32.0f, dimR, dimG, dimB);
        float gx = 1850.0f - lx - 16.0f;
        unsigned cp = ButtonGlyph(h.button);
        if (cp) {
            char gb[4] = { 0 };
            GlyphUTF8(cp, gb);
            float gw = Font::Measure(gb, 36.0f);
            Font::Draw(gb, gx - gw, y + 34.0f, 36.0f, dimR, dimG, dimB);
        } else {
            float bw = Font::Measure(h.button, 32.0f);
            Font::Draw(h.button, gx - bw, y + 32.0f, 32.0f, dimR, dimG, dimB);
        }
        y += 52.0f;
    }
}

} /* namespace WHints */

