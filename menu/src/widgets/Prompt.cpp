/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/App.hpp"
#include "../core/Sfx.hpp"
#include "../core/Qext.hpp"
#include <string.h>
#include <stdio.h>

namespace WPrompt {

    namespace {

        constexpr int kPromptTextMax = 192;
        constexpr int kPromptButtonConfirm = 0;
        constexpr int kPromptButtonCancel = 1;

        /// @brief Modal prompt phase state machine.
        enum class PromptPhase {
            Closed,
            Open,
        };

        /// @brief Modal prompt state, single global.
        struct PromptState {
            bool isOpen = false;
            u64 ignoreMask = 0;
            int selectedButton = 1;
            u64 pendingTitleId = 0;
            char dialogTitle[kPromptTextMax];
            char dialogMessage[kPromptTextMax];
        };

        static PromptState s_state = {};

        /// @brief Current prompt phase derived from state.
        /// @return Closed when dismissed, Open when covering the screen.
        PromptPhase CurrentPhase()
        {
            return s_state.isOpen ? PromptPhase::Open : PromptPhase::Closed;
        }

        /// @brief Copy text with a fallback when null/empty.
        /// @param destinationText Destination buffer.
        /// @param sourceText Source text (may be null).
        /// @param fallbackText Used when source is null or empty.
        static void CopyText(char *destinationText, const char *sourceText, const char *fallbackText)
        {
            if (!sourceText || !sourceText[0]) {
                sourceText = fallbackText;
            }

            if (!sourceText) {
                destinationText[0] = 0;
                return;
            }

            unsigned charIndex = 0;

            while (sourceText[charIndex] != 0 && charIndex + 1 < kPromptTextMax) {
                destinationText[charIndex] = sourceText[charIndex];
                charIndex++;
            }

            destinationText[charIndex] = 0;
        }

    } // namespace

    /// @brief Whether a modal prompt covers the screen.
    /// @return True when open.
    bool IsOpen()
    {
        return CurrentPhase() == PromptPhase::Open;
    }

    /// @brief Give the selected button (0 = left/confirm, 1 = right/cancel).
    /// @return Selection index.
    int Sel()
    {
        return s_state.selectedButton;
    }

    /// @brief Open a generic two-button prompt.
    /// @param dialogTitle Title line (copied).
    /// @param dialogMessage Subtitle line (copied).
    /// @param heldButtons Buttons currently held; disarmed until released so
    /// the opening press can never confirm or dismiss.
    void Open(const char *dialogTitle, const char *dialogMessage, u64 heldButtons)
    {
        CopyText(s_state.dialogTitle, dialogTitle, "Are you sure?");
        CopyText(s_state.dialogMessage, dialogMessage, "");
        s_state.selectedButton = kPromptButtonCancel;
        s_state.pendingTitleId = 0;
        s_state.ignoreMask = heldButtons;
        s_state.isOpen = true;
        Sfx::Play(Sfx::Hover);
    }

    /// @brief Open the close-game confirm for the running title.
    /// @param heldButtons Buttons currently held (see Open).
    /// @param launchTitleId Title to launch when Close is confirmed, 0 for close only.
    void OpenCloseGame(u64 heldButtons, u64 launchTitleId)
    {
        if (CurrentPhase() == PromptPhase::Open) {
            return;
        }

        if (!qext_game_running()) {
            return;
        }

        const char *gameName = 0;
        u64 suspendedTitleId = qext_suspended_title();

        for (int rowIndex = 0; rowIndex < App::RowCount(); rowIndex++) {
            const AppRow &row = App::Row(rowIndex);

            if (row.isGame && row.tid == suspendedTitleId && row.name[0] != 0) {
                gameName = row.name;
                break;
            }
        }

        if (!gameName) {
            int selectedRow = App::Sel();

            if (selectedRow >= 0 && selectedRow < App::RowCount()) {
                const AppRow &row = App::Row(selectedRow);

                if (row.isGame && row.name[0] != 0) {
                    gameName = row.name;
                }
            }
        }

        char dialogTitleBuffer[kPromptTextMax];

        if (gameName && gameName[0] != 0) {
            snprintf(dialogTitleBuffer, sizeof(dialogTitleBuffer), "Close %s?", gameName);
            dialogTitleBuffer[sizeof(dialogTitleBuffer) - 1] = 0;
        } else {
            snprintf(dialogTitleBuffer, sizeof(dialogTitleBuffer), "Close software?");
            dialogTitleBuffer[sizeof(dialogTitleBuffer) - 1] = 0;
        }

        Open(dialogTitleBuffer, "Unsaved progress may be lost.", heldButtons);
        s_state.pendingTitleId = launchTitleId;
        logging::LogLine("[menu] prompt open: %s", dialogTitleBuffer);
    }

    /// @brief Dismiss the prompt without acting.
    void Close()
    {
        s_state.isOpen = false;
        s_state.ignoreMask = 0;
        s_state.pendingTitleId = 0;
        s_state.selectedButton = kPromptButtonCancel;
    }

    /// @brief Draw the dim + centered dialog.
    void Draw()
    {
        if (CurrentPhase() == PromptPhase::Closed) {
            return;
        }

        float menuBarRed = 0.0f;
        float menuBarGreen = 0.0f;
        float menuBarBlue = 0.0f;
        Theme::Color("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);

        float backgroundRed = 0.0f;
        float backgroundGreen = 0.0f;
        float backgroundBlue = 0.0f;
        Theme::Color("bg", &backgroundRed, &backgroundGreen, &backgroundBlue);

        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        Theme::Color("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        Theme::Color("dim", &dimRed, &dimGreen, &dimBlue);

        Gfx::PushOverlayQuad(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.6f);

        float dialogX = 580.0f;
        float dialogY = 340.0f;
        float dialogWidth = 760.0f;
        float dialogHeight = 400.0f;
        float dialogOutline = 8.0f;
        Gfx::PushOverlayPanel(dialogX - dialogOutline, dialogY - dialogOutline, dialogWidth + dialogOutline * 2.0f, dialogHeight + dialogOutline * 2.0f, 24.0f + dialogOutline,
                              menuBarRed, menuBarGreen, menuBarBlue, 1.0f);
        Gfx::PushOverlayPanel(dialogX, dialogY, dialogWidth, dialogHeight, 24.0f, backgroundRed, backgroundGreen, backgroundBlue, 1.0f);

        Font::Centered(s_state.dialogTitle, 960.0f, 440.0f, 40.0f, 700.0f, inkRed, inkGreen, inkBlue);
        Font::Centered(s_state.dialogMessage, 960.0f, 500.0f, 30.0f, 700.0f, dimRed, dimGreen, dimBlue);

        float buttonWidth = 280.0f;
        float buttonHeight = 80.0f;
        float buttonY = 580.0f;
        float buttonOutline = 6.0f;
        float buttonX[2] = { 660.0f, 980.0f };
        const char *buttonLabels[2] = { "Close", "Cancel" };

        for (int buttonIndex = 0; buttonIndex < 2; buttonIndex++) {
            Gfx::PushOverlayPanel(buttonX[buttonIndex] - buttonOutline, buttonY - buttonOutline, buttonWidth + buttonOutline * 2.0f, buttonHeight + buttonOutline * 2.0f,
                                  16.0f + buttonOutline, menuBarRed, menuBarGreen, menuBarBlue, 1.0f);
            Gfx::PushOverlayPanel(buttonX[buttonIndex], buttonY, buttonWidth, buttonHeight, 16.0f, backgroundRed, backgroundGreen, backgroundBlue, 1.0f);

            if (s_state.selectedButton == buttonIndex) {
                Gfx::PushOverlayRing(buttonX[buttonIndex] - 6.0f, buttonY - 6.0f, buttonWidth + 12.0f, buttonHeight + 12.0f,
                                     20.0f, 8.0f);
            }

            Font::Centered(buttonLabels[buttonIndex], buttonX[buttonIndex] + buttonWidth * 0.5f, buttonY + 52.0f, 32.0f, 260.0f,
                            inkRed, inkGreen, inkBlue);
        }
    }

    /// @brief Handle prompt navigation input.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    /// @return True when the input was consumed.
    bool Input(u64 buttonsDown, u64 buttonsHeld)
    {
        switch (CurrentPhase()) {
            case PromptPhase::Closed:
                return false;

            case PromptPhase::Open:
                break;
        }

        s_state.ignoreMask &= buttonsHeld;
        u64 freshPress = buttonsDown & ~s_state.ignoreMask;

        if (freshPress & (HidNpadButton_AnyLeft | HidNpadButton_AnyRight)) {
            s_state.selectedButton ^= 1;
            Sfx::Play(Sfx::Hover);
            return true;
        }

        if (freshPress & HidNpadButton_A) {
            if (s_state.selectedButton == kPromptButtonConfirm) {
                qext_terminate_game();
                u64 pendingLaunchId = s_state.pendingTitleId;
                Close();

                if (pendingLaunchId != 0) {
                    App::LaunchGame(pendingLaunchId);
                } else {
                    Sfx::Play(Sfx::Click);
                }
            } else {
                Close();
                Sfx::Play(Sfx::Back);
            }

            return true;
        }

        if (freshPress & (HidNpadButton_B | HidNpadButton_X)) {
            Close();
            Sfx::Play(Sfx::Back);
            return true;
        }

        return true;
    }

} /* namespace WPrompt */
