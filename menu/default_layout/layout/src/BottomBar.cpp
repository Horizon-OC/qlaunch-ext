/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "MenuHost.hpp"
#include "LayoutHost.hpp"
#include "Gfx.hpp"
#include "Sfx.hpp"

namespace WBottomBar {

    namespace {

        constexpr int kBottomButtonCount = 1;
        constexpr float kBottomBarY = 924.0f;
        constexpr float kBottomBarHeight = 156.0f;
        constexpr float kBottomButtonSize = 72.0f;
        constexpr float kBottomRowStartX = 1806.0f;
        constexpr float kBottomRowY = 958.0f;

        /// @brief Focus phase for the bottom bar action state machine.
        enum class BottomFocusPhase {
            Unfocused,
            Focused,
        };

        /// @brief Current focus phase derived from App state.
        /// @return Unfocused when the bottom bar is hidden or busy.
        BottomFocusPhase CurrentPhase()
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            int homeTabIndex = hostApi->tabFindById("home");

            if (!hostApi->bottomFocus() || hostApi->appMenu() != homeTabIndex) {
                return BottomFocusPhase::Unfocused;
            }

            if (hostApi->homeShiftBusy()) {
                return BottomFocusPhase::Unfocused;
            }

            return BottomFocusPhase::Focused;
        }

    } // namespace

    /// @brief Draw the bottom bar (pad icon + home shortcuts).
    void Draw()
    {
        const MenuHostApi *hostApi = GetLayoutHost();
        hostApi->gfxPushLayer((int)Gfx::GfxLayer::MenuBar);
        float menuBarRed = 0.0f;
        float menuBarGreen = 0.0f;
        float menuBarBlue = 0.0f;
        hostApi->themeColor("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);
        hostApi->gfxPushPanel(0.0f, kBottomBarY, 1920.0f, kBottomBarHeight, 0.0f, menuBarRed, menuBarGreen, menuBarBlue, 1.0f);
        hostApi->gfxPopLayer();

        hostApi->gfxPushLayer((int)Gfx::GfxLayer::MenuBarItems);

        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

        unsigned controllerGlyph = hostApi->padGlyph();
        float controllerRed = 0.0f;
        float controllerGreen = 0.0f;
        float controllerBlue = 0.0f;
        hostApi->padIconColor(&controllerRed, &controllerGreen, &controllerBlue);

        char glyphText[4] = { 0 };
        GlyphUTF8(controllerGlyph, glyphText);
        float glyphWidth = hostApi->fontMeasure(glyphText, 72.0f);
        hostApi->fontDraw(glyphText, 76.0f + (100.0f - glyphWidth) * 0.5f, 957.0f + 62.0f, 72.0f, controllerRed, controllerGreen, controllerBlue, 1.0f);

        int homeTabIndex = hostApi->tabFindById("home");

        if (hostApi->appMenu() == homeTabIndex) {
            float buttonRed = 1.0f;
            float buttonGreen = 1.0f;
            float buttonBlue = 1.0f;

            if (hostApi->themeIsDark()) {
                buttonRed = 0.16f;
                buttonGreen = 0.16f;
                buttonBlue = 0.18f;
            }

            for (int buttonIndex = 0; buttonIndex < kBottomButtonCount; buttonIndex++) {
                bool isFocused = hostApi->bottomFocus() && hostApi->bottomSelection() == buttonIndex;
                float buttonCenterX = kBottomRowStartX;
                float buttonDiameter = kBottomButtonSize + 16.0f;
                hostApi->gfxPushPanel(buttonCenterX - 8.0f, kBottomRowY - 8.0f, buttonDiameter, buttonDiameter, buttonDiameter * 0.5f, buttonRed, buttonGreen, buttonBlue, 1.0f);

                char powerText[4] = { 0 };
                GlyphUTF8(GLYPH_POWER, powerText);
                float powerWidth = hostApi->fontMeasure(powerText, 52.0f);

                float iconRed = 0.0f;
                float iconGreen = 0.0f;
                float iconBlue = 0.0f;
                hostApi->themeColor("icon", &iconRed, &iconGreen, &iconBlue);

                hostApi->fontDraw(powerText, buttonCenterX + (kBottomButtonSize - powerWidth) * 0.5f, kBottomRowY + kBottomButtonSize * 0.5f + 18.0f, 52.0f, iconRed, iconGreen, iconBlue, 1.0f);

                if (isFocused) {
                    hostApi->gfxPushSelectRing(buttonCenterX - 9.0f, kBottomRowY - 9.0f, buttonDiameter + 2.0f, buttonDiameter + 2.0f, (buttonDiameter + 2.0f) * 0.5f, 10.0f);
                }
            }
        }

        hostApi->gfxPopLayer();
    }

    /// @brief Handle bottom-bar navigation input (single sleep button).
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held (unused).
    void Input(u64 buttonsDown, u64 buttonsHeld)
    {
        const MenuHostApi *hostApi = GetLayoutHost();
        (void)buttonsHeld;

        switch (CurrentPhase()) {
            case BottomFocusPhase::Unfocused:
                return;

            case BottomFocusPhase::Focused:
                break;
        }

        if (buttonsDown & HidNpadButton_AnyUp) {
            hostApi->setBottomFocus(0);
            hostApi->sfxPlayByName("hover");
        }

        if (buttonsDown & HidNpadButton_B) {
            hostApi->setBottomFocus(0);
            hostApi->sfxPlayByName("back");
        }
    }

} /* namespace WBottomBar */
