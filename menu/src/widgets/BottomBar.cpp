/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Icons.hpp"
#include "../core/Layout.hpp"
#include "../core/App.hpp"
#include "../core/Sfx.hpp"
#include "../core/PadIcon.hpp"
#include "../core/Input.hpp"
#include "../core/Tabs.hpp"

namespace WBottomBar {

    namespace {

        constexpr int kBottomButtonCount = 3;
        constexpr float kBottomBarY = 924.0f;
        constexpr float kBottomBarHeight = 156.0f;
        constexpr float kBottomButtonSize = 72.0f;
        constexpr float kBottomButtonGap = 24.0f;
        constexpr float kBottomRowStartX = 1578.0f;
        constexpr float kBottomRowY = 958.0f;

        /// @brief Focus phase for the bottom bar action state machine.
        enum class BottomFocusPhase {
            Unfocused,
            Focused,
        };

        /// @brief Bottom-bar transient state (key repeat only).
        struct BottomBarState {
            InputRepeat keyRepeat;
        };

        static BottomBarState s_state = { { 0 } };

        /// @brief Resolve a HUD icon slot.
        /// @param hudId kHud* icon id.
        /// @return Icon slot.
        int ResolveHudSlot(int hudId)
        {
            return Icons::HudSlot(hudId);
        }

        /// @brief Current focus phase derived from App state.
        /// @return Unfocused when the bottom bar is hidden or busy.
        BottomFocusPhase CurrentPhase()
        {
            int homeTabIndex = Tabs::FindById("home");

            if (!App::BottomFocus() || App::Menu() != homeTabIndex) {
                return BottomFocusPhase::Unfocused;
            }

            if (App::HomeShiftBusy()) {
                return BottomFocusPhase::Unfocused;
            }

            return BottomFocusPhase::Focused;
        }

    } // namespace

    /// @brief Draw the bottom bar (pad icon + home shortcuts).
    void Draw()
    {
        float menuBarRed = 0.0f;
        float menuBarGreen = 0.0f;
        float menuBarBlue = 0.0f;
        Theme::Color("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);
        Gfx::PushPanel(0.0f, kBottomBarY, 1920.0f, kBottomBarHeight, 0.0f, menuBarRed, menuBarGreen, menuBarBlue, 1.0f);

        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        Theme::Color("ink", &inkRed, &inkGreen, &inkBlue);

        /* Controller symbol. TODO: add better controller detection. */
        unsigned controllerGlyph = PadIcon::Glyph();
        float controllerRed = 0.0f;
        float controllerGreen = 0.0f;
        float controllerBlue = 0.0f;
        PadIcon::Color(&controllerRed, &controllerGreen, &controllerBlue);

        char glyphText[4] = { 0 };
        GlyphUTF8(controllerGlyph, glyphText);
        float glyphWidth = Font::Measure(glyphText, 72.0f);
        Font::Draw(glyphText, 76.0f + (100.0f - glyphWidth) * 0.5f, 957.0f + 62.0f, 72.0f, controllerRed, controllerGreen, controllerBlue);

        int homeTabIndex = Tabs::FindById("home");

        /* Only on the home tab. */
        if (App::Menu() == homeTabIndex) {
            /* M-FOLDER 1614, M-VGC 1710, sleep 1806. */
            float buttonRed = 1.0f;
            float buttonGreen = 1.0f;
            float buttonBlue = 1.0f;

            if (Theme::IsDark()) {
                buttonRed = 0.16f;
                buttonGreen = 0.16f;
                buttonBlue = 0.18f;
            }

            for (int buttonIndex = 0; buttonIndex < kBottomButtonCount; buttonIndex++) {
                bool isFocused = App::BottomFocus() && App::BottomSel() == buttonIndex;
                float buttonCenterX = kBottomRowStartX + (float)buttonIndex * (kBottomButtonSize + kBottomButtonGap);
                float buttonDiameter = kBottomButtonSize + 16.0f;
                Gfx::PushPanel(buttonCenterX - 8.0f, kBottomRowY - 8.0f, buttonDiameter, buttonDiameter, buttonDiameter * 0.5f, buttonRed, buttonGreen, buttonBlue, 1.0f);

                if (buttonIndex == 2) {
                    char powerText[4] = { 0 };
                    GlyphUTF8(GLYPH_POWER, powerText);
                    float powerWidth = Font::Measure(powerText, 52.0f);

                    float iconRed = 0.0f;
                    float iconGreen = 0.0f;
                    float iconBlue = 0.0f;
                    Theme::Color("icon", &iconRed, &iconGreen, &iconBlue);

                    Font::Draw(powerText, buttonCenterX + (kBottomButtonSize - powerWidth) * 0.5f, kBottomRowY + kBottomButtonSize * 0.5f + 18.0f, 52.0f, iconRed, iconGreen, iconBlue);
                } else {
                    int iconSlot = (buttonIndex == 0) ? ResolveHudSlot(kHudFolder) : ResolveHudSlot(kHudVgcCard);

                    if (Gfx::TexValid(iconSlot)) {
                        Gfx::PushIcon(buttonCenterX, kBottomRowY + 8.0f, kBottomButtonSize, kBottomButtonSize, 16.0f, 16.0f, 16.0f, 16.0f, 1.0f, 1.0f, 1.0f, 0.0f, iconSlot);
                    }
                }

                if (isFocused) {
                    Gfx::PushSelectRing(buttonCenterX - 9.0f, kBottomRowY - 9.0f, buttonDiameter + 2.0f, buttonDiameter + 2.0f, (buttonDiameter + 2.0f) * 0.5f, 10.0f);
                }
            }
        }
    }

    /// @brief Handle bottom-bar navigation input.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void Input(u64 buttonsDown, u64 buttonsHeld)
    {
        switch (CurrentPhase()) {
            case BottomFocusPhase::Unfocused:
                return;

            case BottomFocusPhase::Focused:
                break;
        }

        s_state.keyRepeat.Poll(buttonsDown, buttonsHeld, HidNpadButton_AnyLeft | HidNpadButton_AnyRight, DefaultRepeat());

        if (buttonsDown & HidNpadButton_AnyLeft) {
            int currentSelection = App::BottomSel();

            if (currentSelection > 0) {
                App::SetBottomSel(currentSelection - 1);
                Sfx::Play(Sfx::Hover);
            }
        }

        if (buttonsDown & HidNpadButton_AnyRight) {
            int currentSelection = App::BottomSel();

            if (currentSelection < 2) {
                App::SetBottomSel(currentSelection + 1);
                Sfx::Play(Sfx::Hover);
            }
        }

        if (buttonsDown & HidNpadButton_AnyUp) {
            App::SetBottomFocus(false);
            Sfx::Play(Sfx::Hover);
        }

        if (buttonsDown & HidNpadButton_B) {
            App::SetBottomFocus(false);
            Sfx::Play(Sfx::Back);
        }
    }

} /* namespace WBottomBar */
