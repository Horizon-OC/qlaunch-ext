/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */

#include "Widgets.hpp"
#include "../core/App.hpp"
#include "../core/Qext.hpp"
#include "../core/Sfx.hpp"
#include "../core/Touch.hpp"
#include <string.h>
#include <stdio.h>

namespace WPrompt {

    namespace {

        constexpr int kPromptTextMax = 192;
        constexpr int kPromptConfirmIndex = 0;

        /// @brief Close-game confirm state, single global.
        struct PromptState {
            bool isOpen = false;
            u64 pendingTitleId = 0;
        };

        static PromptState s_state = {};

    } // namespace

    /// @brief Whether the close-game confirm covers the screen.
    /// @return True when open.
    bool IsOpen()
    {
        return s_state.isOpen;
    }

    /// @brief Open the close-game confirm for the running title.
    /// Confirming Close terminates the game; when launchTid is set the
    /// title is launched right after. Cancelling does nothing.
    /// @param heldButtons Buttons currently held (disarmed until released so
    /// the opening press can never confirm or dismiss).
    /// @param launchTitleId Title to launch when Close is confirmed, 0 for close only.
    void OpenCloseGame(u64 heldButtons, u64 launchTitleId)
    {
        if (s_state.isOpen || WPopup::IsOpen()) {
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

        static const char *closeLabels[2] = { "Close", "Cancel" };
        s_state.pendingTitleId = launchTitleId;
        s_state.isOpen = true;
        WPopup::Open(WPopup::PopupLayout::Row, dialogTitleBuffer, "Unsaved progress may be lost.",
                     closeLabels, 0, 2, 1, heldButtons);
        Sfx::PlayByName("hover");
    }

    /// @brief Dismiss the prompt without acting.
    void Close()
    {
        if (!s_state.isOpen) {
            return;
        }

        s_state.isOpen = false;
        s_state.pendingTitleId = 0;
        WPopup::Close();
    }

    /// @brief Handle prompt navigation input.
    /// Taps hit-test the dialog buttons directly: tapping a button
    /// selects and presses it, tapping outside cancels.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    /// @return True when the input was consumed.
    bool Input(u64 buttonsDown, u64 buttonsHeld)
    {
        if (!s_state.isOpen) {
            return false;
        }

        float tapX = 0.0f;
        float tapY = 0.0f;

        while (Touch::TakeTap(&tapX, &tapY)) {
            int tappedCell = WPopup::CellAt(tapX, tapY);

            if (tappedCell >= 0) {
                WPopup::SetSelection(tappedCell);
                buttonsDown |= HidNpadButton_A;
            } else {
                buttonsDown |= HidNpadButton_B;
            }
        }

        WPopup::PopupResult popupResult = WPopup::Input(buttonsDown, buttonsHeld);

        if (popupResult == WPopup::PopupResult::Confirmed) {
            if (WPopup::Selection() == kPromptConfirmIndex) {
                qext_terminate_game();
                u64 pendingLaunchId = s_state.pendingTitleId;
                Close();

                if (pendingLaunchId != 0) {
                    App::LaunchGame(pendingLaunchId);
                } else {
                    Sfx::PlayByName("click");
                }
            } else {
                Close();
                Sfx::PlayByName("back");
            }
        } else if (popupResult == WPopup::PopupResult::Cancelled) {
            Close();
            Sfx::PlayByName("back");
        }

        return true;
    }

} /* namespace WPrompt */
