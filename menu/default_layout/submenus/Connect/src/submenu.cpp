/* NekoMenu Connect submenu (C) 2026 Souldbminer */
/* Licensed under the GPLv2                */

#include <switch.h>
#include "MenuHost.hpp"
#include "SubmenuApi.hpp"
#include <cstdio>
#include <stdint.h>

extern const uint8_t top_sel_connect_png[];
extern const uint32_t top_sel_connect_png_size;
extern const uint8_t top_des_connect_png[];
extern const uint32_t top_des_connect_png_size;

namespace {

    constexpr int kConnectFriendCount = 8;
    constexpr float kConnectTileSize = 200.0f;
    constexpr float kConnectTileGap = 50.0f;
    constexpr unsigned kConnectRepeatHold = 20;
    constexpr unsigned kConnectRepeatPeriod = 6;

    enum class ConnectAction {
        Idle,
        MoveLeft,
        MoveRight,
        ReleaseToTopBar,
    };

    struct ConnectState {
        int selectedFriend;
        unsigned repeatCounter;
    };

    static ConnectState s_state = { 0, 0 };

    /// @brief Poll directional buttons with key repeat.
    /// @param buttonsDown Proven pressed mask, updated with repeats.
    /// @param buttonsHeld Held mask.
    void PollRepeat(unsigned long long &buttonsDown, unsigned long long buttonsHeld)
    {
        constexpr unsigned long long directionMask =
            (unsigned long long)HidNpadButton_AnyLeft | (unsigned long long)HidNpadButton_AnyRight;
        unsigned long long heldDirections = buttonsHeld & directionMask;

        if (buttonsDown & directionMask) {
            s_state.repeatCounter = 0;
            return;
        }

        if (heldDirections) {
            s_state.repeatCounter++;

            if (s_state.repeatCounter > kConnectRepeatHold &&
                (s_state.repeatCounter % kConnectRepeatPeriod) == 0) {
                buttonsDown |= heldDirections;
            }
        } else {
            s_state.repeatCounter = 0;
        }
    }

} // namespace

extern "C" {

    /// @brief Give the submenu ABI version.
    /// @return kSubmenuApiVersion.
    QLAUNCH_HOOK int SubmenuApiVersion(void)
    {
        return kSubmenuApiVersion;
    }

    /// @brief Give the stable submenu id.
    /// @return Lowercase id owned by the dnro.
    QLAUNCH_HOOK const char *SubmenuId(void)
    {
        return "connect";
    }

    /// @brief Give the display title.
    /// @return Title owned by the dnro.
    QLAUNCH_HOOK const char *SubmenuTitle(void)
    {
        return "Friends";
    }

    /// @brief Give the jingle clip name, if any.
    /// @return Clip name or NULL.
    QLAUNCH_HOOK const char *SubmenuJingle(void)
    {
        return "chat_jingle";
    }

    /// @brief Give the fallback glyph, if any.
    /// @return Codepoint or 0.
    QLAUNCH_HOOK unsigned SubmenuGlyph(void)
    {
        return 0xE132;
    }

    /// @brief Give the embedded icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL.
    QLAUNCH_HOOK const unsigned char *SubmenuIconPng(unsigned *outputSize)
    {
        if (outputSize) {
            *outputSize = top_sel_connect_png_size;
        }

        return top_sel_connect_png;
    }

    /// @brief Give the embedded selected-state icon PNG bytes, if any.
    /// @param outputSize Output byte size.
    /// @return PNG bytes or NULL to reuse the normal icon.
    QLAUNCH_HOOK const unsigned char *SubmenuSelectedIconPng(unsigned *outputSize)
    {
        if (outputSize) {
            *outputSize = top_des_connect_png_size;
        }

        return top_des_connect_png;
    }

    /// @brief Give the submenu accent color.
    /// @param hostApi Host API for theme-derived colors.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return Nonzero when a color is provided.
    QLAUNCH_HOOK int SubmenuColor(const MenuHostApi *hostApi,
                                  float *redOut, float *greenOut, float *blueOut)
    {
        if (!hostApi || !redOut || !greenOut || !blueOut) {
            return 0;
        }

        hostApi->themeTabAccent("connect", redOut, greenOut, blueOut);
        return 1;
    }

    /// @brief Give the submenu background color.
    /// @param hostApi Host API for theme-derived colors.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return Nonzero when a color is provided.
    QLAUNCH_HOOK int SubmenuBackgroundColor(const MenuHostApi *hostApi,
                                            float *redOut, float *greenOut, float *blueOut)
    {
        if (!hostApi || !redOut || !greenOut || !blueOut) {
            return 0;
        }

        hostApi->themeTabColor("connect", redOut, greenOut, blueOut);
        return 1;
    }

    /// @brief Boot the submenu with the menu host API.
    /// @param hostApi All menu functions plus the qext family.
    /// @return 0 to stay loaded.
    QLAUNCH_HOOK int SubmenuBoot(const MenuHostApi *hostApi)
    {
        if (!hostApi || hostApi->version != kMenuHostApiVersion) {
            return -1;
        }

        s_state.selectedFriend = 0;
        s_state.repeatCounter = 0;
        return 0;
    }

    /// @brief Draw the submenu onto the core's state.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuDraw(const MenuHostApi *hostApi)
    {
        float slideOffsetX = hostApi->layoutShiftX();

        float inkRed = 0.0f;
        float inkGreen = 0.0f;
        float inkBlue = 0.0f;
        hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

        float dimRed = 0.0f;
        float dimGreen = 0.0f;
        float dimBlue = 0.0f;
        hostApi->themeColor("dim", &dimRed, &dimGreen, &dimBlue);

        hostApi->fontDraw("Friends", 120.0f + slideOffsetX, 280.0f, 44.0f, inkRed, inkGreen, inkBlue, 1.0f);

        float rowStartX = 120.0f + slideOffsetX;
        float rowY = 340.0f;

        for (int friendIndex = 0; friendIndex < kConnectFriendCount; friendIndex++) {
            float tileX = rowStartX + (float)friendIndex * (kConnectTileSize + kConnectTileGap);

            if (tileX > 1900.0f) {
                break;
            }

            bool isSelected = (s_state.selectedFriend == friendIndex);

            float tileRed = 0.0f;
            float tileGreen = 0.0f;
            float tileBlue = 0.0f;

            if (isSelected) {
                hostApi->themeColor("accent", &tileRed, &tileGreen, &tileBlue);
            } else {
                hostApi->themeColor("panel", &tileRed, &tileGreen, &tileBlue);
            }

            hostApi->gfxPushPanel(tileX, rowY, kConnectTileSize, kConnectTileSize, 100.0f,
                                  tileRed, tileGreen, tileBlue, 1.0f);

            char friendName[16];
            snprintf(friendName, sizeof(friendName), "Friend %d", friendIndex + 1);
            float nameWidth = hostApi->fontMeasure(friendName, 28.0f);
            hostApi->fontDraw(friendName, tileX + (kConnectTileSize - nameWidth) * 0.5f,
                              rowY + kConnectTileSize + 40.0f, 28.0f, dimRed, dimGreen, dimBlue, 1.0f);
        }
    }

    /// @brief Handle submenu input.
    /// @param hostApi Host API.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    QLAUNCH_HOOK void SubmenuInput(const MenuHostApi *hostApi,
                                   unsigned long long buttonsDown,
                                   unsigned long long buttonsHeld)
    {
        PollRepeat(buttonsDown, buttonsHeld);

        ConnectAction action = ConnectAction::Idle;

        if (buttonsDown & (unsigned long long)HidNpadButton_AnyLeft) {
            action = ConnectAction::MoveLeft;
        } else if (buttonsDown & (unsigned long long)HidNpadButton_AnyRight) {
            action = ConnectAction::MoveRight;
        } else if (buttonsDown & (unsigned long long)HidNpadButton_AnyUp) {
            action = ConnectAction::ReleaseToTopBar;
        }

        switch (action) {
            case ConnectAction::Idle:
                break;

            case ConnectAction::MoveLeft: {
                if (s_state.selectedFriend > 0) {
                    s_state.selectedFriend--;
                    hostApi->sfxPlayByName("hover");
                }
                break;
            }

            case ConnectAction::MoveRight: {
                if (s_state.selectedFriend < kConnectFriendCount - 1) {
                    s_state.selectedFriend++;
                    hostApi->sfxPlayByName("hover");
                }
                break;
            }

            case ConnectAction::ReleaseToTopBar:
                hostApi->setTopSelection(hostApi->appMenu());
                hostApi->setTopFocus(1);
                hostApi->sfxPlayByName("hover");
                break;
        }
    }

    /// @brief Enter the submenu tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuOpen(const MenuHostApi *hostApi)
    {
        (void)hostApi;
    }

    /// @brief Exit the submenu tab.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuClose(const MenuHostApi *hostApi)
    {
        (void)hostApi;
    }

    /// @brief Tick the submenu once per frame.
    /// @param hostApi Host API.
    QLAUNCH_HOOK void SubmenuTick(const MenuHostApi *hostApi)
    {
        (void)hostApi;
    }

    /// @brief Shut down before unload.
    QLAUNCH_HOOK void SubmenuShutdown(void)
    {
    }

} /* extern C */
