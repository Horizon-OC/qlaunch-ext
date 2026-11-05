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

        constexpr float kBottomBarY = 924.0f;
        constexpr float kBottomBarHeight = 156.0f;
        constexpr float kBottomButtonSize = 72.0f;
        constexpr float kBottomButtonGap = 24.0f;
        constexpr float kBottomRowRightX = 1806.0f;
        constexpr float kBottomRowY = 958.0f;
        constexpr int kBottomBarButtonsMax = 17;

        /// @brief Focus phase for the bottom bar action state machine.
        enum class BottomFocusPhase {
            Unfocused,
            Focused,
        };

        /// @brief Collect the active tab's plugin button globals, in order.
        /// Sleep is not included; the caller appends it last on home.
        /// @param hostApi Host API.
        /// @param tabIndex Active tab registry index.
        /// @param globalsOut Output global button indices.
        /// @param globalCap Output capacity.
        /// @return Plugin button count for the tab.
        int TabPluginButtons(const MenuHostApi *hostApi, int tabIndex, int *globalsOut, int globalCap)
        {
            int tabButtons = 0;
            int totalButtons = hostApi->menuButtonCount();

            for (int buttonIndex = 0; buttonIndex < totalButtons && tabButtons < globalCap; buttonIndex++) {
                if (hostApi->menuButtonTab(buttonIndex) == tabIndex) {
                    globalsOut[tabButtons++] = buttonIndex;
                }
            }

            return tabButtons;
        }

        /// @brief Whether the active tab shows any bottom-bar buttons.
        /// @param hostApi Host API.
        /// @return True on home or when plugins target the active tab.
        bool HasBarButtons(const MenuHostApi *hostApi)
        {
            int homeTabIndex = hostApi->tabFindById("home");

            if (hostApi->appMenu() == homeTabIndex) {
                return true;
            }

            int globals[kBottomBarButtonsMax];
            return TabPluginButtons(hostApi, hostApi->appMenu(), globals, kBottomBarButtonsMax) > 0;
        }

        /// @brief Current focus phase derived from App state.
        /// @return Unfocused when the bottom bar is hidden or busy.
        BottomFocusPhase CurrentPhase()
        {
            const MenuHostApi *hostApi = GetLayoutHost();

            if (!hostApi->bottomFocus() || !HasBarButtons(hostApi)) {
                return BottomFocusPhase::Unfocused;
            }

            if (hostApi->homeShiftBusy()) {
                return BottomFocusPhase::Unfocused;
            }

            return BottomFocusPhase::Focused;
        }

    } // namespace

    /// @brief Draw the bottom bar (pad icon plus tab buttons).
    /// Shows plugin buttons for the active tab with the sleep button
    /// last on home; labels sit under plugin buttons.
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
        int currentTab = hostApi->appMenu();
        bool onHome = (currentTab == homeTabIndex);
        int pluginGlobals[kBottomBarButtonsMax];
        int pluginCount = TabPluginButtons(hostApi, currentTab, pluginGlobals, kBottomBarButtonsMax);
        int sleepCount = onHome ? 1 : 0;
        int buttonCount = pluginCount + sleepCount;

        if (onHome || pluginCount > 0) {
            float buttonRed = 1.0f;
            float buttonGreen = 1.0f;
            float buttonBlue = 1.0f;

            if (hostApi->themeIsDark()) {
                buttonRed = 0.16f;
                buttonGreen = 0.16f;
                buttonBlue = 0.18f;
            }

            float rowStartX = kBottomRowRightX - (float)(buttonCount - 1) * (kBottomButtonSize + kBottomButtonGap);

            for (int buttonIndex = 0; buttonIndex < buttonCount; buttonIndex++) {
                bool isPlugin = buttonIndex < pluginCount;
                bool isFocused = hostApi->bottomFocus() && hostApi->bottomSelection() == buttonIndex;
                float buttonCenterX = rowStartX + (float)buttonIndex * (kBottomButtonSize + kBottomButtonGap);
                float buttonDiameter = kBottomButtonSize + 16.0f;
                hostApi->gfxPushPanel(buttonCenterX - 8.0f, kBottomRowY - 8.0f, buttonDiameter, buttonDiameter, buttonDiameter * 0.5f, buttonRed, buttonGreen, buttonBlue, 1.0f);

                if (isPlugin) {
                    int globalButton = pluginGlobals[buttonIndex];
                    int iconSlot = hostApi->menuButtonIcon(globalButton);

                    if (hostApi->gfxTexValid(iconSlot)) {
                        hostApi->gfxPushIcon(buttonCenterX, kBottomRowY + 8.0f, kBottomButtonSize, kBottomButtonSize, 16.0f, 16.0f, 16.0f, 16.0f, 1.0f, 1.0f, 1.0f, 0.0f, iconSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                    } else {
                        float iconRed = 0.0f;
                        float iconGreen = 0.0f;
                        float iconBlue = 0.0f;
                        hostApi->themeColor("icon", &iconRed, &iconGreen, &iconBlue);

                        const char *label = hostApi->menuButtonLabel(globalButton);
                        hostApi->fontCentered(label ? label : "", buttonCenterX + kBottomButtonSize * 0.5f,
                                              kBottomRowY + kBottomButtonSize * 0.5f + 8.0f, 20.0f,
                                              kBottomButtonSize - 8.0f, iconRed, iconGreen, iconBlue, 1.0f);
                    }

                    const char *label = hostApi->menuButtonLabel(globalButton);

                    if (label && label[0]) {
                        hostApi->fontCentered(label, buttonCenterX + kBottomButtonSize * 0.5f,
                                              kBottomRowY + kBottomButtonSize + 26.0f, 20.0f, 132.0f,
                                              inkRed, inkGreen, inkBlue, 1.0f);
                    }
                } else {
                    char powerText[4] = { 0 };
                    GlyphUTF8(GLYPH_POWER, powerText);
                    float powerWidth = hostApi->fontMeasure(powerText, 52.0f);

                    float iconRed = 0.0f;
                    float iconGreen = 0.0f;
                    float iconBlue = 0.0f;
                    hostApi->themeColor("icon", &iconRed, &iconGreen, &iconBlue);

                    hostApi->fontDraw(powerText, buttonCenterX + (kBottomButtonSize - powerWidth) * 0.5f, kBottomRowY + kBottomButtonSize * 0.5f + 18.0f, 52.0f, iconRed, iconGreen, iconBlue, 1.0f);
                }

                if (isFocused) {
                    hostApi->gfxPushSelectRing(buttonCenterX - 9.0f, kBottomRowY - 9.0f, buttonDiameter + 2.0f, buttonDiameter + 2.0f, (buttonDiameter + 2.0f) * 0.5f, 10.0f);
                }
            }
        }

        hostApi->gfxPopLayer();
    }

    /// @brief Handle bottom-bar navigation input (plugin buttons + sleep).
    /// Left/right move across the row; up or B releases focus.
    /// A activates through App::ActivateBottom.
    /// @param buttonsDown Proven pressed mask.
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

        int homeTabIndex = hostApi->tabFindById("home");
        int pluginGlobals[kBottomBarButtonsMax];
        int pluginCount = TabPluginButtons(hostApi, hostApi->appMenu(), pluginGlobals, kBottomBarButtonsMax);
        int buttonCount = pluginCount + (hostApi->appMenu() == homeTabIndex ? 1 : 0);

        if (buttonsDown & HidNpadButton_AnyLeft) {
            int currentSelection = hostApi->bottomSelection();

            if (currentSelection > 0) {
                hostApi->setBottomSelection(currentSelection - 1);
                hostApi->sfxPlayByName("hover");
            }
        }

        if (buttonsDown & HidNpadButton_AnyRight) {
            int currentSelection = hostApi->bottomSelection();

            if (currentSelection < buttonCount - 1) {
                hostApi->setBottomSelection(currentSelection + 1);
                hostApi->sfxPlayByName("hover");
            }
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
