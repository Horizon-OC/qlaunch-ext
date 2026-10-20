/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>
#include "../core/Layout.hpp"
#include "../core/Tabs.hpp"

/// @brief Widget collection. Popup/Prompt are generic host services;
/// chrome and strip widgets live in layout.dnro and home.dnro.

namespace WBackground {
    /// @brief Draw the fullscreen background for the active (or outgoing) tab.
    /// @param layoutNode Layout node (reserved for JSON-driven colors).
    void Draw(const LayoutNode &layoutNode);
} /* namespace WBackground */

namespace WTopBar {
    /// @brief Draw the top bar (avatars, tab icons, clock, status).
    /// @param layoutNode Layout node (currently unused, geometry is fixed).
    void Draw(const LayoutNode &layoutNode);

    /// @brief Handle top-bar navigation input.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void Input(u64 buttonsDown, u64 buttonsHeld);

    /// @brief Hit-test a design-space point against the tab strip.
    /// Uses the same measurement as Draw, so art and touch agree.
    /// @param x X in design space.
    /// @param y Y in design space.
    /// @param tabIndexOut Tabs registry index on hit.
    /// @return True when a tab was hit.
    bool TabAtPosition(float x, float y, int *tabIndexOut);
} /* namespace WTopBar */

namespace WBottomBar {
    /// @brief Draw the bottom bar (pad icon + home shortcuts).
    void Draw();

    /// @brief Handle bottom-bar navigation input.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void Input(u64 buttonsDown, u64 buttonsHeld);
} /* namespace WBottomBar */

namespace WStrip {
    /// @brief Draw the home game strip from a layout node.
    /// @param layoutNode Gamestrip node carrying tile geometry.
    void Draw(const LayoutNode &layoutNode);

    /// @brief Draw folders/VGC sub-screens under the strip.
    void DrawSubs();

    /// @brief Handle home strip navigation input.
    /// @param layoutNode Gamestrip node (reserved).
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void Input(const LayoutNode &layoutNode, u64 buttonsDown, u64 buttonsHeld);

    /// @brief Describe the Home tab for the transplantable tab registry.
    /// @return MenuTab descriptor for Home.
    struct MenuTab DescribeTab();

    /// @brief Whether the title options menu is open.
    /// @return True while the popup captures input.
    bool OptionsOpen();

    /// @brief Close the title options menu without acting.
    void CloseOptions();
} /* namespace WStrip */

namespace WHints {
    /// @brief Draw footer hints from the layout node, else fallback state hints.
    /// @param layoutNode Layout node carrying optional hint items.
    void Draw(const LayoutNode &layoutNode);
} /* namespace WHints */

namespace WPrompt {
    /// @brief Whether the close-game confirm covers the screen.
    /// @return True when open.
    bool IsOpen();

    /// @brief Open the close-game confirm for the running title.
    /// Confirming Close terminates the game; when launchTid is set the
    /// title is launched right after. Cancelling does nothing.
    /// @param buttonsHeld Buttons currently held (disarmed until released so
    /// the opening press can never confirm or dismiss).
    /// @param launchTid Title to launch when Close is confirmed, 0 for close only.
    void OpenCloseGame(u64 buttonsHeld, u64 launchTid);

    /// @brief Dismiss the prompt without acting.
    void Close();

    /// @brief Handle prompt navigation input.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    /// @return True when the input was consumed.
    bool Input(u64 buttonsDown, u64 buttonsHeld);
} /* namespace WPrompt */

namespace WPopup {
    constexpr int kPopupMaxItems = 12;
    constexpr int kPopupTextMax = 192;

    /// @brief Popup arrangement.
    enum class PopupLayout : int {
        List,
        Row,
    };

    /// @brief Single-frame popup input outcome.
    enum class PopupResult : int {
        None,
        Confirmed,
        Cancelled,
    };

    /// @brief Open a modal popup, replacing any current one.
    /// Labels are copied, so caller buffers may be transient.
    /// @param layout List (vertical rows) or Row (side-by-side buttons).
    /// @param titleText Title line ("Are you sure?" when null/empty).
    /// @param messageText Subtitle line (may be null).
    /// @param labels Item labels (capped at kPopupMaxItems).
    /// @param swatchColors Per-item swatch RGB triples, or null for text only.
    /// @param labelCount Item count.
    /// @param initialSelection Initially highlighted item.
    /// @param buttonsHeld Buttons currently held; disarmed until released so
    /// the opening press can never confirm or dismiss.
    void Open(PopupLayout layout, const char *titleText, const char *messageText,
              const char * const *labels, const float *swatchColors, int labelCount,
              int initialSelection, u64 buttonsHeld);

    /// @brief Dismiss the popup without acting.
    void Close();

    /// @brief Arrange List items in columns (1 keeps the single column).
    /// Open resets this to 1, so set it after opening a grid popup.
    /// @param columns Column count (below 1 reads as 1).
    void SetGridColumns(int columns);

    /// @brief Whether a modal popup covers the screen.
    /// @return True when open.
    bool IsOpen();

    /// @brief Give the highlighted item.
    /// @return Selection index.
    int Selection();

    /// @brief Hit-test a design-space point against popup items.
    /// Mirrors Draw geometry exactly.
    /// @param x X in design space.
    /// @param y Y in design space.
    /// @return Item index, or -1 when the point hits no item.
    int CellAt(float x, float y);

    /// @brief Highlight one popup item.
    /// @param index Item index (clamped).
    void SetSelection(int index);

    /// @brief Draw the dim + outlined dialog.
    void Draw();

    /// @brief Handle popup navigation input.
    /// @param buttonsDown Proven pressed mask.
    /// @param buttonsHeld Held mask.
    /// @return Confirmed on A, Cancelled on B/X, else None.
    PopupResult Input(u64 buttonsDown, u64 buttonsHeld);
} /* namespace WPopup */

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
