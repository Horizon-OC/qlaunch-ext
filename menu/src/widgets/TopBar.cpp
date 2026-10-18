/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Clock.hpp"
#include "../core/Icons.hpp"
#include "../core/Qext.hpp"
#include "../core/Layout.hpp"
#include "../core/App.hpp"
#include "../core/Sfx.hpp"
#include "../core/Avatars.hpp"
#include "../core/Tabs.hpp"
#include "../core/Input.hpp"
#include "../core/DynamicWidgets.hpp"
#include "../core/Submenus.hpp"
#include <stdio.h>
#include <string.h>

namespace WTopBar {

    namespace {

        constexpr float kTopBarHeight = 201.0f;
        constexpr float kTopBarTabWidth = 98.0f;
        constexpr float kTopBarTabGap = 5.0f;
        constexpr float kTopBarSelectedMargin = 5.0f;
        constexpr float kTopBarTabY = 67.0f;
        constexpr int kTopBarMaxAvatars = 2;

        /// @brief Focus phase for the top bar action state machine.
        enum class TopBarPhase {
            Inactive,
            Navigating,
        };

        /// @brief Per-widget transient state, single global.
        struct TopBarState {
            float recolorProgress;
            int lastFocusedTab;
            InputRepeat keyRepeat;
        };

        static TopBarState s_state = { 0.0f, -2, { 0 } };

        /// @brief Current phase derived from App focus state.
        /// @return Navigating when the top bar owns focus.
        TopBarPhase CurrentPhase()
        {
            return App::TopFocus() ? TopBarPhase::Navigating : TopBarPhase::Inactive;
        }

        /// @brief Resolve the art slot for a tab.
        /// @param tabIndex Tabs registry index.
        /// @param isActive True when the tab is current.
        /// @return HUD icon slot, or <= 0 when missing.
        int TopArt(int tabIndex, bool isActive)
        {
            const MenuTab *tabDescriptor = Tabs::Get(tabIndex);

            if (!tabDescriptor) {
                return -1;
            }

            if (tabDescriptor->hudSel > 0 && tabDescriptor->hudDesel > 0) {
                return isActive ? tabDescriptor->hudDesel : tabDescriptor->hudSel;
            }

            int selectedId = -1;
            int deselectedId = -1;

            if (strcmp(tabDescriptor->id, "home") == 0) {
                selectedId = kHudTopSelHome;
                deselectedId = kHudTopDesHome;
            } else if (strcmp(tabDescriptor->id, "album") == 0) {
                selectedId = kHudTopSelAlbum;
                deselectedId = kHudTopDesAlbum;
            } else {
                return -1;
            }

            return Icons::HudSlot(isActive ? deselectedId : selectedId);
        }

        /// @brief Fallback glyph for a tab.
        /// @param tabIndex Tabs registry index.
        /// @return Unicode codepoint, or 0.
        unsigned TabGlyph(int tabIndex)
        {
            const MenuTab *tabDescriptor = Tabs::Get(tabIndex);

            if (tabDescriptor && tabDescriptor->glyph) {
                return tabDescriptor->glyph;
            }

            if (!tabDescriptor) {
                return 0;
            }

            if (strcmp(tabDescriptor->id, "home") == 0) {
                return GLYPH_HOME;
            }

            if (strcmp(tabDescriptor->id, "connect") == 0) {
                return GLYPH_CHAT;
            }

            if (strcmp(tabDescriptor->id, "eshop") == 0) {
                return GLYPH_ESHOP;
            }

            if (strcmp(tabDescriptor->id, "album") == 0) {
                return GLYPH_ALBUM;
            }

            if (strcmp(tabDescriptor->id, "settings") == 0) {
                return GLYPH_GEAR;
            }

            return 0;
        }

        /// @brief Recolor a section with the S2 gradient.
        /// @param animationPhase 0-1 animation phase.
        /// @param red Red output.
        /// @param green Green output.
        /// @param blue Blue output.
        void Recolor(float animationPhase, float *red, float *green, float *blue)
        {
            static const float palette[4][3] = {
                { 0.537f, 0.584f, 0.945f },
                { 0.384f, 0.588f, 0.949f },
                { 0.863f, 0.722f, 0.875f },
                { 1.0f, 1.0f, 1.0f },
            };

            float segment = animationPhase * 4.0f;
            int segmentIndex = (int)segment;
            int paletteIndex = segmentIndex & 3;
            float blend = segment - (float)segmentIndex;
            const float *firstColor = palette[paletteIndex];
            const float *secondColor = palette[(paletteIndex + 1) & 3];
            *red = firstColor[0] + (secondColor[0] - firstColor[0]) * blend;
            *green = firstColor[1] + (secondColor[1] - firstColor[1]) * blend;
            *blue = firstColor[2] + (secondColor[2] - firstColor[2]) * blend;
        }

        /// @brief Draw the avatar cluster on the left.
        void DrawAvatars()
        {
            float dimRed = 0.0f;
            float dimGreen = 0.0f;
            float dimBlue = 0.0f;
            Theme::Color("dim", &dimRed, &dimGreen, &dimBlue);

            int avatarCount = Avatars::Count();

            if (avatarCount > kTopBarMaxAvatars) {
                avatarCount = kTopBarMaxAvatars;
            }

            for (int avatarIndex = 0; avatarIndex < avatarCount; avatarIndex++) {
                float avatarX = 75.0f + (float)avatarIndex * 103.0f;
                Gfx::PushPanel(avatarX, 66.0f, 92.0f, 92.0f, 46.0f, dimRed, dimGreen, dimBlue, 1.0f);

                int avatarSlot = Avatars::Slot(avatarIndex);

                if (!Gfx::TexValid(avatarSlot)) {
                    avatarSlot = Icons::HudSlot(kHudUser);
                }

                if (Gfx::TexValid(avatarSlot)) {
                    Gfx::PushIcon(avatarX + 9.0f, 75.0f, 74.0f, 74.0f, 30.0f, 30.0f, 30.0f, 30.0f, 1.0f, 1.0f, 1.0f, 0.0f, avatarSlot);
                }
            }
        }

        /// @brief Draw the centered tab icon strip.
        /// @param currentTab Current menu tab index.
        void DrawTabStrip(int currentTab)
        {
            float iconRed = 0.0f;
            float iconGreen = 0.0f;
            float iconBlue = 0.0f;
            Theme::Color("icon", &iconRed, &iconGreen, &iconBlue);

            float baseWidth = kTopBarTabWidth;
            float activeWidth = baseWidth * 1.4f;
            int tabCount = Tabs::Count();

            if (tabCount <= 0) {
                tabCount = kMenuCount;
            }

            int selectedCount = 0;

            for (int tabIndex = 0; tabIndex < tabCount; tabIndex++) {
                bool isTabActive = (tabIndex == currentTab);
                bool isTabFocused = App::TopFocus() && App::TopSel() == tabIndex;

                if (isTabActive || isTabFocused) {
                    selectedCount++;
                }
            }

            float totalWidth = baseWidth * (float)(tabCount - 1) + activeWidth + kTopBarTabGap * (float)(tabCount - 1);
            totalWidth += kTopBarSelectedMargin * 2.0f * (float)selectedCount;
            float cursorX = (1920.0f - totalWidth) * 0.5f;

            for (int tabIndex = 0; tabIndex < tabCount; tabIndex++) {
                bool isActive = (tabIndex == currentTab);
                bool isFocused = App::TopFocus() && App::TopSel() == tabIndex;
                bool isSelected = isActive || isFocused;

                float slotWidth = isActive ? activeWidth : baseWidth;
                float iconSize = isActive ? slotWidth : baseWidth * (isFocused ? 1.2f : 1.0f);

                if (isSelected) {
                    cursorX += kTopBarSelectedMargin;
                }

                float centerX = cursorX + slotWidth * 0.5f;
                float iconX = centerX - iconSize * 0.5f;
                float iconY = kTopBarTabY + (baseWidth - iconSize) * 0.5f;
                cursorX += slotWidth + kTopBarTabGap;

                if (isSelected) {
                    cursorX += kTopBarSelectedMargin;
                }

                int artSlot = TopArt(tabIndex, isActive);

                const MenuTab *tintTab = Tabs::Get(tabIndex);
                float tintRed = 0.5f;
                float tintGreen = 0.5f;
                float tintBlue = 0.5f;

                if (tintTab && Submenus::IsDynamicTab(tabIndex)) {
                    if (!Submenus::TabColor(tabIndex, &tintRed, &tintGreen, &tintBlue)) {
                        tintRed = 0.5f;
                        tintGreen = 0.5f;
                        tintBlue = 0.5f;
                    }
                } else if (tintTab) {
                    Theme::MenuAccent(Theme::MenuSlotForId(tintTab->id), &tintRed, &tintGreen, &tintBlue);
                }

                if (isFocused) {
                    Recolor(s_state.recolorProgress, &tintRed, &tintGreen, &tintBlue);
                } else if (!isActive) {
                    tintRed = iconRed;
                    tintGreen = iconGreen;
                    tintBlue = iconBlue;
                }

                if (Gfx::TexValid(artSlot)) {
                    Gfx::PushIcon(iconX, iconY, iconSize, iconSize, 0.0f, 0.0f, 0.0f, 0.0f, tintRed, tintGreen, tintBlue, 0.0f, artSlot);
                } else {
                    unsigned glyphCodepoint = TabGlyph(tabIndex);

                    if (!glyphCodepoint) {
                        continue;
                    }

                    char glyphText[4] = { 0 };
                    GlyphUTF8(glyphCodepoint, glyphText);
                    float glyphSize = iconSize * 0.62f;
                    float glyphWidth = Font::Measure(glyphText, glyphSize);
                    Font::Draw(glyphText, iconX + (iconSize - glyphWidth) * 0.5f, iconY + iconSize * 0.5f + glyphSize * 0.35f, glyphSize, tintRed, tintGreen, tintBlue);
                }
            }
        }


        /// @brief Draw the builtin clock text.
        /// @param inkRed Ink red.
        /// @param inkGreen Ink green.
        /// @param inkBlue Ink blue.
        void DrawClockWidget(float inkRed, float inkGreen, float inkBlue)
        {
            char timeText[16];
            snprintf(timeText, sizeof(timeText), "%s", Clock::TimeStr());
            char meridiemText[8];
            snprintf(meridiemText, sizeof(meridiemText), "%s", Clock::AmPm());
            float timeWidth = Font::Measure(timeText, 40.0f);
            float meridiemWidth = Font::Measure(meridiemText, 26.0f);
            float clockX = 1694.0f - timeWidth - 8.0f - meridiemWidth;
            Font::Draw(timeText, clockX, 130.0f, 40.0f, inkRed, inkGreen, inkBlue);
            Font::Draw(meridiemText, clockX + timeWidth + 8.0f, 130.0f, 26.0f, inkRed, inkGreen, inkBlue);
        }

        /// @brief Draw the builtin wifi icon.
        /// @param inkRed Ink red.
        /// @param inkGreen Ink green.
        /// @param inkBlue Ink blue.
        /// @param iconRed Icon red.
        /// @param iconGreen Icon green.
        /// @param iconBlue Icon blue.
        void DrawWifiWidget(float inkRed, float inkGreen, float inkBlue, float iconRed, float iconGreen, float iconBlue)
        {
            int wifiSlot = Icons::HudSlot(kHudWifiNone);
            float wifiRed = inkRed * 0.5f;
            float wifiGreen = inkGreen * 0.5f;
            float wifiBlue = inkBlue * 0.5f;

            if (Clock::Plane() > 0) {
                wifiSlot = Icons::HudSlot(kHudAirplane);
                wifiRed = iconRed;
                wifiGreen = iconGreen;
                wifiBlue = iconBlue;
            } else {
                int connectionType = Clock::NetType();
                int signalBars = Clock::Wifi();
                bool isConnected = (connectionType == QEXT_NET_WIFI && signalBars >= 1 && signalBars <= 3) ||
                                   connectionType == QEXT_NET_ETHERNET;

                if (connectionType == QEXT_NET_ETHERNET) {
                    wifiSlot = Icons::HudSlot(kHudWifi0);
                } else if (connectionType == QEXT_NET_WIFI) {
                    int clampedSignal = signalBars;

                    if (clampedSignal < 0) {
                        clampedSignal = 0;
                    }

                    if (clampedSignal > 3) {
                        clampedSignal = 3;
                    }

                    wifiSlot = Icons::HudSlot(kHudWifi0 + (3 - clampedSignal));
                }

                if (isConnected) {
                    wifiRed = iconRed;
                    wifiGreen = iconGreen;
                    wifiBlue = iconBlue;
                }
            }

            if (Gfx::TexValid(wifiSlot)) {
                Gfx::PushIcon(1717.0f, 88.0f, 51.0f, 51.0f, 0.0f, 0.0f, 0.0f, 0.0f, wifiRed, wifiGreen, wifiBlue, 0.0f, wifiSlot);
            } else {
                char wifiGlyph[4] = { 0 };
                GlyphUTF8(GLYPH_WIFI, wifiGlyph);
                Font::Draw(wifiGlyph, 1717.0f, 130.0f, 44.0f, wifiRed, wifiGreen, wifiBlue);
            }
        }

        /// @brief Draw the builtin battery outline and fill.
        /// @param inkRed Ink red.
        /// @param inkGreen Ink green.
        /// @param inkBlue Ink blue.
        /// @param menuBarRed Menu bar red.
        /// @param menuBarGreen Menu bar green.
        /// @param menuBarBlue Menu bar blue.
        void DrawBatteryWidget(float inkRed, float inkGreen, float inkBlue, float menuBarRed, float menuBarGreen, float menuBarBlue)
        {
            unsigned batteryPercent = Clock::Batt();

            if (batteryPercent > 100) {
                batteryPercent = 100;
            }

            float fillRatio = (float)batteryPercent / 100.0f;

            float fillRed = 0.0f;
            float fillGreen = 0.0f;
            float fillBlue = 0.0f;

            if (Clock::Charging() > 0) {
                Theme::Color("good", &fillRed, &fillGreen, &fillBlue);
            } else {
                fillRed = fillGreen = fillBlue = 1.0f;
            }

            Gfx::PushPanel(1790.0f, 95.0f, 64.0f, 36.0f, 8.0f, inkRed, inkGreen, inkBlue, 1.0f);
            Gfx::PushPanel(1795.0f, 100.0f, 54.0f, 26.0f, 5.0f, menuBarRed, menuBarGreen, menuBarBlue, 1.0f);

            if (fillRatio > 0.0f) {
                Gfx::PushPanel(1796.0f, 101.0f, 52.0f * fillRatio, 24.0f, 4.0f, fillRed, fillGreen, fillBlue, 1.0f);
            }

            Gfx::PushPanel(1854.0f, 104.0f, 8.0f, 18.0f, 3.0f, inkRed, inkGreen, inkBlue, 1.0f);
        }

        /// @brief Draw the clock, wifi, and battery cluster on the right.
        void DrawStatusCluster()
        {
            float inkRed = 0.0f;
            float inkGreen = 0.0f;
            float inkBlue = 0.0f;
            Theme::Color("ink", &inkRed, &inkGreen, &inkBlue);

            float menuBarRed = 0.0f;
            float menuBarGreen = 0.0f;
            float menuBarBlue = 0.0f;
            Theme::Color("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);

            float iconRed = 0.0f;
            float iconGreen = 0.0f;
            float iconBlue = 0.0f;
            Theme::Color("icon", &iconRed, &iconGreen, &iconBlue);

            if (!DynamicWidgets::HasWidget("clock")) {
                DrawClockWidget(inkRed, inkGreen, inkBlue);
            }

            if (!DynamicWidgets::HasWidget("wifi")) {
                DrawWifiWidget(inkRed, inkGreen, inkBlue, iconRed, iconGreen, iconBlue);
            }

            if (!DynamicWidgets::HasWidget("battery")) {
                DrawBatteryWidget(inkRed, inkGreen, inkBlue, menuBarRed, menuBarGreen, menuBarBlue);
            }

            DynamicWidgets::DrawAll();
        }

    } // namespace

    /// @brief Draw the top bar (avatars, tab icons, clock, status).
    /// @param layoutNode Layout node (currently unused, geometry is fixed).
    void Draw(const LayoutNode &layoutNode)
    {
        (void)layoutNode;
        Gfx::PushLayer(Gfx::GfxLayer::MenuBar);

        int focusedTabIndex = App::TopFocus() ? App::TopSel() : -1;

        if (focusedTabIndex != s_state.lastFocusedTab) {
            s_state.lastFocusedTab = focusedTabIndex;
            s_state.recolorProgress = 0.0f;
        } else {
            s_state.recolorProgress += 0.5f / 60.0f;

            if (s_state.recolorProgress >= 1.0f) {
                s_state.recolorProgress -= 1.0f;
            }
        }

        float menuBarRed = 0.0f;
        float menuBarGreen = 0.0f;
        float menuBarBlue = 0.0f;
        Theme::Color("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);
        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, kTopBarHeight, 0.0f, menuBarRed, menuBarGreen, menuBarBlue, 1.0f);
        Gfx::PopLayer();

        Gfx::PushLayer(Gfx::GfxLayer::MenuBarItems);
        DrawAvatars();
        DrawTabStrip(App::Menu());
        DrawStatusCluster();
        Gfx::PopLayer();
    }

    /// @brief Handle top-bar navigation input.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void Input(u64 buttonsDown, u64 buttonsHeld)
    {
        switch (CurrentPhase()) {
            case TopBarPhase::Inactive:
                return;

            case TopBarPhase::Navigating:
                break;
        }

        int tabCount = Tabs::Count();

        if (tabCount <= 0) {
            return;
        }

        s_state.keyRepeat.Poll(buttonsDown, buttonsHeld, HidNpadButton_AnyLeft | HidNpadButton_AnyRight, DefaultRepeat());

        int currentSelection = App::TopSel();

        if (buttonsDown & HidNpadButton_AnyLeft) {
            if (currentSelection > 0) {
                App::SetTopSel(currentSelection - 1);
                Sfx::Play(Sfx::Hover);
            }
        }

        if (buttonsDown & HidNpadButton_AnyRight) {
            if (currentSelection < tabCount - 1) {
                App::SetTopSel(currentSelection + 1);
                Sfx::Play(Sfx::Hover);
            }
        }

        if (buttonsDown & HidNpadButton_AnyDown) {
            App::SetTopFocus(false);
            Sfx::Play(Sfx::Hover);
        }

        if (buttonsDown & HidNpadButton_B) {
            App::SetTopFocus(false);
            Sfx::Play(Sfx::Back);
        }
    }

} /* namespace WTopBar */
