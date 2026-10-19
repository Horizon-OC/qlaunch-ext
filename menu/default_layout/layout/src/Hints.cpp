/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "MenuHost.hpp"
#include "LayoutHost.hpp"
#include "Gfx.hpp"
#include "Icons.hpp"
#include "Layout.hpp"
#include "App.hpp"
#include <string.h>

/* TODO: Do we remove this or keep it somewhere? (alternate mode?)*/
namespace WHints {

    namespace {

        /// @brief Maximum hints drawn from a layout node or fallback.
        constexpr int kHintsMax = 3;

        /// @brief One resolved footer hint.
        struct ResolvedHint {
            const char *button;
            const char *label;
        };

        /// @brief Map a button name to its glyph fallback.
        /// @param buttonName Button name.
        /// @return Glyph codepoint, or 0.
        unsigned ButtonGlyph(const char *buttonName)
        {
            if (strcmp(buttonName, "A") == 0) {
                return GLYPH_A;
            }

            if (strcmp(buttonName, "B") == 0) {
                return GLYPH_B;
            }

            if (strcmp(buttonName, "X") == 0) {
                return GLYPH_X;
            }

            if (strcmp(buttonName, "Y") == 0) {
                return GLYPH_Y;
            }

            if (strcmp(buttonName, "+") == 0) {
                return GLYPH_PLUS;
            }

            if (strcmp(buttonName, "-") == 0) {
                return GLYPH_MINUS;
            }

            return 0;
        }

        /// @brief Pick fallback hints from app focus state.
        /// @param outputHints Output buffer.
        /// @return Number of hints written.
        int FallbackHints(ResolvedHint *outputHints)
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            int hintCount = 0;
            int homeTabIndex = hostApi->tabFindById("home");
            int settingsTabIndex = hostApi->tabFindById("settings");
            int selectedRow = hostApi->appSelection();
            const AppRow *selectedRowData = selectedRow >= 0 && selectedRow < hostApi->rowCount() ?
                hostApi->rowAt(selectedRow) : nullptr;

            if (hostApi->topFocus()) {
                outputHints[hintCount++] = { "A", "OK" };
                outputHints[hintCount++] = { "B", "Back" };
            } else if (hostApi->appMenu() == settingsTabIndex) {
                outputHints[hintCount++] = { "A", "OK" };
                outputHints[hintCount++] = { "B", "Back" };
            } else if (hostApi->appMenu() == homeTabIndex && hostApi->bottomFocus()) {
                outputHints[hintCount++] = { "A", "OK" };
                outputHints[hintCount++] = { "B", "Back" };
            } else if (hostApi->appMenu() == homeTabIndex && hostApi->homeSub() != kHomeSubGames) {
                outputHints[hintCount++] = { "A", "Start" };
                outputHints[hintCount++] = { "Y", "Options" };
                outputHints[hintCount++] = { "B", "Back" };
            } else if (hostApi->popupIsOpen()) {
                outputHints[hintCount++] = { "A", "OK" };
                outputHints[hintCount++] = { "B", "Back" };
            } else if (selectedRowData && selectedRowData->isFolder) {
                outputHints[hintCount++] = { "A", "Open" };
                outputHints[hintCount++] = { "Y", "Rename" };
            } else {
                outputHints[hintCount++] = { "A", "Start" };
                outputHints[hintCount++] = { "Y", "Folder" };
            }

            return hintCount;
        }

    } // namespace

    /// @brief Draw footer hints from the layout node, else fallback state hints.
    /// @param layoutNode Layout node carrying optional hint items.
    void Draw(const LayoutNode &layoutNode)
    {
        const MenuHostApi *hostApi = GetLayoutHost();
        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        hostApi->themeColor("dim", &dimRed, &dimGreen, &dimBlue);

        /* Footer divider. */
        hostApi->gfxPushPanel(60.0f, 940.0f, 1680.0f, 3.0f, 0.0f, dimRed, dimGreen, dimBlue, 1.0f);

        ResolvedHint resolvedHints[kHintsMax];
        int resolvedCount = 0;

        if (layoutNode.hintCount > 0) {
            for (int hintIndex = 0; hintIndex < layoutNode.hintCount && resolvedCount < kHintsMax; hintIndex++) {
                resolvedHints[resolvedCount].button = layoutNode.hints[hintIndex].button;
                resolvedHints[resolvedCount].label = layoutNode.hints[hintIndex].label;
                resolvedCount++;
            }
        } else {
            resolvedCount = FallbackHints(resolvedHints);
        }

        float baselineY = 978.0f;
        float cursorX = 1740.0f;

        for (int hintIndex = resolvedCount - 1; hintIndex >= 0; hintIndex--) {
            const char *button = resolvedHints[hintIndex].button;
            const char *label = resolvedHints[hintIndex].label;

            float labelWidth = hostApi->fontMeasure(label, 36.0f);
            hostApi->fontDraw(label, cursorX - labelWidth, baselineY, 36.0f, dimRed, dimGreen, dimBlue, 1.0f);

            cursorX -= labelWidth + 14.0f;
            int iconSlot = -1;

            if (strcmp(button, "A") == 0) {
                iconSlot = hostApi->iconsHudSlot(kHudButtonA);
            } else if (strcmp(button, "+") == 0) {
                iconSlot = hostApi->iconsHudSlot(kHudButtonPlus);
            }

            if (hostApi->gfxTexValid(iconSlot)) {
                float menuBarRed = 0.0f;
                float menuBarGreen = 0.0f;
                float menuBarBlue = 0.0f;
                hostApi->themeColor("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);
                hostApi->gfxPushIcon(cursorX - 40.0f, baselineY - 34.0f, 40.0f, 40.0f, 0.0f, 0.0f, 0.0f, 0.0f, menuBarRed, menuBarGreen, menuBarBlue, 0.0f, iconSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                cursorX -= 40.0f + 40.0f;
            } else {
                unsigned glyphCodepoint = ButtonGlyph(button);

                if (glyphCodepoint) {
                    char glyphText[4] = { 0 };
                    GlyphUTF8(glyphCodepoint, glyphText);
                    float glyphWidth = hostApi->fontMeasure(glyphText, 38.0f);
                    hostApi->fontDraw(glyphText, cursorX - glyphWidth, baselineY - 2.0f, 38.0f, dimRed, dimGreen, dimBlue, 1.0f);
                    cursorX -= glyphWidth + 40.0f;
                } else {
                    float buttonWidth = hostApi->fontMeasure(button, 36.0f);
                    hostApi->fontDraw(button, cursorX - buttonWidth, baselineY, 36.0f, dimRed, dimGreen, dimBlue, 1.0f);
                    cursorX -= buttonWidth + 40.0f;
                }
            }
        }
    }

} /* namespace WHints */
