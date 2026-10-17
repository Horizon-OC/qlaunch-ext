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

        constexpr int kTextMax = 192;

        /// @brief Modal prompt state
        struct State {
            bool open = false;
            u64 ignoreMask = 0;
            int sel = 1;
            u64 pendingTid = 0;
            char title[kTextMax];
            char message[kTextMax];
        };

        static State s_state = {};

        /// @brief Copy text with a fallback when null/empty.
        /// @param dst Destination buffer.
        /// @param src Source text (may be null).
        /// @param fallback Used when src is null or empty.
        static void CopyText(char *dst, const char *src, const char *fallback)
        {
            if (!src || !src[0]) {
                src = fallback;
            }

            if (!src) {
                dst[0] = 0;
                return;
            }

            unsigned i = 0;

            while (src[i] != 0 && i + 1 < kTextMax) {
                dst[i] = src[i];
                i++;
            }

            dst[i] = 0;
        }

    } // namespace

    /// @brief Whether a modal prompt covers the screen.
    /// @return True when open.
    bool IsOpen()
    {
        return s_state.open;
    }

    /// @brief Give the selected button (0 = left/confirm, 1 = right/cancel).
    /// @return Selection index.
    int Sel()
    {
        return s_state.sel;
    }

    /// @brief Open a generic two-button prompt.
    /// @param title Title line (copied).
    /// @param message Subtitle line (copied).
    /// @param held Buttons currently held; disarmed until released so
    /// the opening press can never confirm or dismiss.
    void Open(const char *title, const char *message, u64 held)
    {
        CopyText(s_state.title, title, "Are you sure?");
        CopyText(s_state.message, message, "");
        s_state.sel = 1;
        s_state.pendingTid = 0;
        s_state.ignoreMask = held;
        s_state.open = true;
        Sfx::Play(Sfx::Hover);
    }

    /// @brief Open the close-game confirm for the running title.
    /// @param held Buttons currently held (see Open).
    /// @param launchTid Title to launch when Close is confirmed, 0 for close only.
    void OpenCloseGame(u64 held, u64 launchTid)
    {
        if (s_state.open) {
            return;
        }

        if (!qext_game_running()) {
            return;
        }

        const char *name = 0;
        u64 susp = qext_suspended_title();

        for (int i = 0; i < App::RowCount(); i++) {
            const AppRow &r = App::Row(i);

            if (r.isGame && r.tid == susp && r.name[0] != 0) {
                name = r.name;
                break;
            }
        }

        if (!name) {
            int sel = App::Sel();

            if (sel >= 0 && sel < App::RowCount()) {
                const AppRow &r = App::Row(sel);

                if (r.isGame && r.name[0] != 0) {
                    name = r.name;
                }
            }
        }

        char title[kTextMax];

        if (name && name[0] != 0) {
            snprintf(title, sizeof(title), "Close %s?", name);
            title[sizeof(title) - 1] = 0;
        } else {
            snprintf(title, sizeof(title), "Close software?");
            title[sizeof(title) - 1] = 0;
        }

        Open(title, "Unsaved progress may be lost.", held);
        s_state.pendingTid = launchTid;
        logging::LogLine("[menu] prompt open: %s", title);
    }

    /// @brief Dismiss the prompt without acting.
    void Close()
    {
        s_state.open = false;
        s_state.ignoreMask = 0;
        s_state.pendingTid = 0;
        s_state.sel = 1;
    }

    /// @brief Draw the dim + centered dialog.
    void Draw()
    {
        if (!s_state.open) {
            return;
        }

        float barR, barG, barB;
        Theme::Color("menubar", &barR, &barG, &barB);

        float bgR, bgG, bgB;
        Theme::Color("bg", &bgR, &bgG, &bgB);

        float inkR, inkG, inkB;
        Theme::Color("ink", &inkR, &inkG, &inkB);

        float dimR, dimG, dimB;
        Theme::Color("dim", &dimR, &dimG, &dimB);

        Gfx::PushOverlayQuad(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 0.6f);

        float x = 580.0f, y = 340.0f, w = 760.0f, h = 400.0f, o = 8.0f;
        Gfx::PushOverlayPanel(x - o, y - o, w + o * 2.0f, h + o * 2.0f, 24.0f + o,
                              barR, barG, barB, 1.0f);
        Gfx::PushOverlayPanel(x, y, w, h, 24.0f, bgR, bgG, bgB, 1.0f);

        Font::Centered(s_state.title, 960.0f, 440.0f, 40.0f, 700.0f, inkR, inkG, inkB);
        Font::Centered(s_state.message, 960.0f, 500.0f, 30.0f, 700.0f, dimR, dimG, dimB);

        float bw = 280.0f, bh = 80.0f, by = 580.0f, bo = 6.0f;
        float bx[2] = { 660.0f, 980.0f };
        const char *labels[2] = { "Close", "Cancel" };

        for (int i = 0; i < 2; i++) {
            Gfx::PushOverlayPanel(bx[i] - bo, by - bo, bw + bo * 2.0f, bh + bo * 2.0f,
                                  16.0f + bo, barR, barG, barB, 1.0f);
            Gfx::PushOverlayPanel(bx[i], by, bw, bh, 16.0f, bgR, bgG, bgB, 1.0f);

            if (s_state.sel == i) {
                Gfx::PushOverlayRing(bx[i] - 6.0f, by - 6.0f, bw + 12.0f, bh + 12.0f,
                                     20.0f, 8.0f);
            }

            Font::Centered(labels[i], bx[i] + bw * 0.5f, by + 52.0f, 32.0f, 260.0f,
                            inkR, inkG, inkB);
        }
    }

    /// @brief Handle prompt navigation input.
    /// @param down Buttons pressed.
    /// @param held Buttons held.
    /// @return True when the input was consumed.
    bool Input(u64 down, u64 held)
    {
        if (!s_state.open) {
            return false;
        }

        s_state.ignoreMask &= held;
        u64 edge = down & ~s_state.ignoreMask;

        if (edge & (HidNpadButton_AnyLeft | HidNpadButton_AnyRight)) {
            s_state.sel ^= 1;
            Sfx::Play(Sfx::Hover);
            return true;
        }

        if (edge & HidNpadButton_A) {
            if (s_state.sel == 0) {
                qext_terminate_game();
                u64 launch = s_state.pendingTid;
                Close();

                if (launch != 0) {
                    App::LaunchGame(launch);
                } else {
                    Sfx::Play(Sfx::Click);
                }
            } else {
                Close();
                Sfx::Play(Sfx::Back);
            }

            return true;
        }

        if (edge & (HidNpadButton_B | HidNpadButton_X)) {
            Close();
            Sfx::Play(Sfx::Back);
            return true;
        }

        return true;
    }

} /* namespace WPrompt */
