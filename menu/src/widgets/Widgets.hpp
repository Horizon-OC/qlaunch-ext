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
    void Input(u64 down, u64 held);
} /* namespace WTopBar */

namespace WBottomBar {
    void Draw();
    void Input(u64 down, u64 held);
} /* namespace WBottomBar */

namespace WStrip {
    void Draw(const LayoutNode &nd);
    void DrawSubs();
    void Input(const LayoutNode &nd, u64 down, u64 held);
} /* namespace WStrip */

namespace WSettings {
    void Draw();
    void Input(u64 down, u64 held);
} /* namespace WSettings */

namespace WConnect {
    void Draw();
    void Input(u64 down, u64 held);
} /* namespace WConnect */

namespace WEShop {
    void Draw();
    void Input(u64 down, u64 held);
} /* namespace WEShop */

namespace WHints {
    void Draw(const LayoutNode &nd);
} /* namespace WHints */

namespace WAlbum {
    bool IsOpen();
    void Open();
    void Close();
    void Draw();
    bool Input(u64 down, u64 held);
    void OnRefresh();
} /* namespace WAlbum */

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
    GLYPH_HANDHELD = 0xE121,
    GLYPH_WIFI = 0xE024,
    /* P1 gamepad */
    GLYPH_PAD_LEFT = 0xE123,
    GLYPH_PAD_RIGHT = 0xE124,
    GLYPH_PAD_LEFT2 = 0xE125,
    GLYPH_PAD_RIGHT2 = 0xE126,
    GLYPH_PAD_RETRO1 = 0xE127,
    GLYPH_PAD_RETRO2 = 0xE128,
    GLYPH_PAD_RETRO3 = 0xE129,
    GLYPH_PAD_SWITCH = 0xE12A,
    GLYPH_PAD_GRIP = 0xE12B,
    GLYPH_PAD_PRO = 0xE12C,
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