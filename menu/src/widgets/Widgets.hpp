/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>
#include "../core/Layout.hpp"

namespace WBackground {
void Draw(const LayoutNode &nd);
} /* namespace WBackground */

namespace WTopBar {
void Draw(const LayoutNode &nd);
} /* namespace WTopBar */

namespace WStrip {
void Draw(const LayoutNode &nd);
void Input(const LayoutNode &nd, u64 down, u64 held);
} /* namespace WStrip */

namespace WDock {
void Draw(const LayoutNode &nd);
void Input(const LayoutNode &nd, u64 down, u64 held);
} /* namespace WDock */

namespace WHints {
void Draw(const LayoutNode &nd);
} /* namespace WHints */

enum : unsigned {
    GLYPH_A = 0xE0A0,
    GLYPH_B = 0xE0A1,
    GLYPH_X = 0xE0A2,
    GLYPH_Y = 0xE0A3,
    GLYPH_PLUS = 0xE045,
    GLYPH_MINUS = 0xE046,
    GLYPH_POWER = 0xE040,
    GLYPH_HOME = 0xE044,
    GLYPH_GEAR = 0xE130,
    GLYPH_ALBUM = 0xE134,
    GLYPH_ESHOP = 0xE133,
    GLYPH_CHAT = 0xE132,
    GLYPH_NEWS = 0xE137,
    GLYPH_JOYCON = 0xE122,
    GLYPH_WIFI = 0xE027,
};

inline int GlyphUTF8(unsigned cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
}

