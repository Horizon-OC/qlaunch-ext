/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>
#include "../core/Layout.hpp"

/// @brief Widget collection. Each namespace is one plugin-driven tab
/// (or chrome). Tabs are registered in Tabs::RegisterBuiltin() and
/// ordered by layout.json; Layout dispatches Draw/Input to them.

namespace WBackground {
    /// @brief Draw the fullscreen background for the active (or outgoing) tab.
    /// @param nd Layout node (reserved for JSON-driven colors).
    void Draw(const LayoutNode &nd);
} /* namespace WBackground */

namespace WTopBar {
    /// @brief Draw the top bar (avatars, tab icons, clock, status).
    /// @param nd Layout node (currently unused, geometry is fixed).
    void Draw(const LayoutNode &nd);

    /// @brief Handle top-bar navigation input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    void Input(u64 down, u64 held);
} /* namespace WTopBar */

namespace WBottomBar {
    /// @brief Draw the bottom bar (pad icon + home shortcuts).
    void Draw();

    /// @brief Handle bottom-bar navigation input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    void Input(u64 down, u64 held);
} /* namespace WBottomBar */

namespace WStrip {
    /// @brief Draw the home game strip from a layout node.
    /// @param nd Gamestrip node carrying tile geometry.
    void Draw(const LayoutNode &nd);

    /// @brief Draw folders/VGC sub-screens under the strip.
    void DrawSubs();

    /// @brief Handle home strip navigation input.
    /// @param nd Gamestrip node (reserved).
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    void Input(const LayoutNode &nd, u64 down, u64 held);
} /* namespace WStrip */

namespace WSettings {
    /// @brief Draw the settings option list and detail pane.
    void Draw();

    /// @brief Handle settings navigation input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    void Input(u64 down, u64 held);
} /* namespace WSettings */

namespace WConnect {
    /// @brief Draw the friends strip.
    void Draw();

    /// @brief Handle friends navigation input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    void Input(u64 down, u64 held);
} /* namespace WConnect */

namespace WEShop {
    /// @brief Draw the shop grid or the news list.
    void Draw();

    /// @brief Handle shop toggle input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    void Input(u64 down, u64 held);
} /* namespace WEShop */

namespace WHints {
    /// @brief Draw footer hints from the layout node, else fallback state hints.
    /// @param nd Layout node carrying optional hint items.
    void Draw(const LayoutNode &nd);
} /* namespace WHints */

namespace WAlbum {
    /// @brief Whether the album viewer covers the screen.
    /// @return True when grid-fullscreen or video is open.
    bool IsFull();

    /// @brief Whether the album tab is open.
    /// @return True when open.
    bool IsOpen();

    /// @brief Open the album grid and refresh thumbnails.
    void Open();

    /// @brief Open one image fullscreen (images only, not movies).
    /// @param albumIndex Album entry index.
    void OpenFull(int albumIndex);

    /// @brief Drop the fullscreen image texture.
    void DropFull();

    /// @brief Drop all thumbnail textures.
    void DropThumbs();

    /// @brief Close the album viewer and free its buffers.
    void Close();

    /// @brief Draw the active viewer phase.
    void Draw();

    /// @brief Handle album grid/full navigation input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    /// @return True when the input was consumed.
    bool Input(u64 down, u64 held);

    /// @brief Drop cached textures after an external refresh.
    void OnRefresh();

} /* namespace WAlbum */

namespace WVideo {
    /// @brief Whether a video is currently open.
    /// @return True when open.
    bool IsOpen();

    /// @brief Open a movie entry for playback.
    /// @param albumIndex Album entry index.
    /// @return True on success.
    bool Open(int albumIndex);

    /// @brief Close the video player.
    void Close();

    /// @brief Put new frames on GPU. Call each tick
    void Tick();

    /// @brief Give the playback position in seconds.
    /// @return Seconds from the start, clamped to the duration.
    double Position();

    /// @brief Whether playback is paused.
    /// @return True when paused.
    bool Paused();

    /// @brief Whether a non-looping clip reached its end.
    /// @return True when ended.
    bool Ended();

    /// @brief Drop pinned video textures.
    void DropPinned();

    /// @brief Draw the current video frame and OSD.
    void Draw();

    /// @brief Handle video playback input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    /// @return True when the input was consumed.
    bool Input(u64 down, u64 held);

    /// @brief Probe (and cache) a movie's duration.
    /// @param albumIndex Album entry index.
    /// @return Duration in seconds, or 0 when unknown.
    double Duration(int albumIndex);

    /// @brief Clear cached durations.
    void DurReset();
} // namespace WVideo

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

/// @brief Encode a codepoint as UTF-8.
/// @param cp Unicode codepoint.
/// @param out Output buffer (at least 3 bytes).
/// @return Bytes written.
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
