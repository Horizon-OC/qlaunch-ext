/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "MenuHost.hpp"
#include "LayoutHost.hpp"
#include "Gfx.hpp"
#include "Theme.hpp"
#include "Icons.hpp"
#include "Qext.hpp"
#include "Layout.hpp"
#include "Sfx.hpp"
#include "Input.hpp"
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
            return GetLayoutHost()->topFocus() ? TopBarPhase::Navigating : TopBarPhase::Inactive;
        }

        /// @brief Resolve the art slot for a tab.
        /// @param tabIndex Tabs registry index.
        /// @param isActive True when the tab is current.
        /// @return HUD icon slot, or <= 0 when missing.
        int TopArt(int tabIndex, bool isActive)
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            int hudSelect = hostApi->tabHudSelect(tabIndex);
            int hudDeselect = hostApi->tabHudDeselect(tabIndex);

            if (hudSelect > 0 && hudDeselect > 0) {
                return isActive ? hudDeselect : hudSelect;
            }

            const char *tabId = hostApi->tabIdAt(tabIndex);

            if (!tabId) {
                return -1;
            }

            int selectedId = -1;
            int deselectedId = -1;

            if (strcmp(tabId, "home") == 0) {
                selectedId = kHudTopSelHome;
                deselectedId = kHudTopDesHome;
            } else if (strcmp(tabId, "album") == 0) {
                selectedId = kHudTopSelAlbum;
                deselectedId = kHudTopDesAlbum;
            } else {
                return -1;
            }

            return hostApi->iconsHudSlot(isActive ? deselectedId : selectedId);
        }

        /// @brief Fallback glyph for a tab.
        /// @param tabIndex Tabs registry index.
        /// @return Unicode codepoint, or 0.
        unsigned TabGlyph(int tabIndex)
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            unsigned glyph = (unsigned)hostApi->tabGlyph(tabIndex);

            if (glyph) {
                return glyph;
            }

            const char *tabId = hostApi->tabIdAt(tabIndex);

            if (!tabId) {
                return 0;
            }

            if (strcmp(tabId, "home") == 0) {
                return GLYPH_HOME;
            }

            if (strcmp(tabId, "connect") == 0) {
                return GLYPH_CHAT;
            }

            if (strcmp(tabId, "eshop") == 0) {
                return GLYPH_ESHOP;
            }

            if (strcmp(tabId, "album") == 0) {
                return GLYPH_ALBUM;
            }

            if (strcmp(tabId, "settings") == 0) {
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
            static const char *keys[4] = { "ring0", "ring1", "ring2", "ring3" };
            static const float fallback[4][3] = {
                { 0.537f, 0.584f, 0.945f },
                { 0.384f, 0.588f, 0.949f },
                { 0.863f, 0.722f, 0.875f },
                { 1.0f, 1.0f, 1.0f },
            };
            float stops[4][3];

            const MenuHostApi *hostApi = GetLayoutHost();

            for (int stop = 0; stop < 4; stop++) {
                stops[stop][0] = fallback[stop][0];
                stops[stop][1] = fallback[stop][1];
                stops[stop][2] = fallback[stop][2];
                hostApi->themeTryColor(keys[stop], &stops[stop][0], &stops[stop][1], &stops[stop][2]);
            }

            float segment = animationPhase * 4.0f;
            int segmentIndex = (int)segment;
            int paletteIndex = segmentIndex & 3;
            float blend = segment - (float)segmentIndex;
            const float *firstColor = stops[paletteIndex];
            const float *secondColor = stops[(paletteIndex + 1) & 3];
            *red = firstColor[0] + (secondColor[0] - firstColor[0]) * blend;
            *green = firstColor[1] + (secondColor[1] - firstColor[1]) * blend;
            *blue = firstColor[2] + (secondColor[2] - firstColor[2]) * blend;
        }

        /// @brief Draw the avatar cluster on the left.
        void DrawAvatars()
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            float dimRed = 0.0f;
            float dimGreen = 0.0f;
            float dimBlue = 0.0f;
            hostApi->themeColor("dim", &dimRed, &dimGreen, &dimBlue);

            int avatarCount = hostApi->avatarCount();

            if (avatarCount > kTopBarMaxAvatars) {
                avatarCount = kTopBarMaxAvatars;
            }

            for (int avatarIndex = 0; avatarIndex < avatarCount; avatarIndex++) {
                float avatarX = 75.0f + (float)avatarIndex * 103.0f;
                hostApi->gfxPushPanel(avatarX, 66.0f, 92.0f, 92.0f, 46.0f, dimRed, dimGreen, dimBlue, 1.0f);

                int avatarSlot = hostApi->avatarSlot(avatarIndex);

                if (!hostApi->gfxTexValid(avatarSlot)) {
                    avatarSlot = hostApi->iconsHudSlot(kHudUser);
                }

                if (hostApi->gfxTexValid(avatarSlot)) {
                    hostApi->gfxPushIcon(avatarX + 9.0f, 75.0f, 74.0f, 74.0f, 30.0f, 30.0f, 30.0f, 30.0f, 1.0f, 1.0f, 1.0f, 0.0f, avatarSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                }
            }
        }

        /// @brief Resolve a visible slot to a Tabs registry index.
        /// Falls back to identity order when the layout reports no slots.
        /// @param slotIndex Layout slot.
        /// @param slotCount Slot count driving the fallback.
        /// @return Tabs registry index.
        int ResolveSlotTab(int slotIndex, int slotCount)
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            int tabIndex = hostApi->layoutTabAt(slotIndex);

            if (tabIndex >= 0) {
                return tabIndex;
            }

            if (slotIndex >= 0 && slotIndex < slotCount) {
                return slotIndex;
            }

            return -1;
        }

        /// @brief Draw the centered tab icon strip in layout slot order.
        /// @param currentTab Current menu tab index.
        void DrawTabStrip(int currentTab)
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            float iconRed = 0.0f;
            float iconGreen = 0.0f;
            float iconBlue = 0.0f;
            hostApi->themeColor("icon", &iconRed, &iconGreen, &iconBlue);

            float baseWidth = kTopBarTabWidth;
            float activeWidth = baseWidth * 1.4f;
            int slotCount = hostApi->layoutTabCount();

            if (slotCount <= 0) {
                slotCount = hostApi->tabCount();
            }

            int selectedCount = 0;

            for (int slotIndex = 0; slotIndex < slotCount; slotIndex++) {
                int tabIndex = ResolveSlotTab(slotIndex, slotCount);
                bool isTabActive = (tabIndex == currentTab);
                bool isTabFocused = hostApi->topFocus() && hostApi->topSelection() == tabIndex;

                if (isTabActive || isTabFocused) {
                    selectedCount++;
                }
            }

            float totalWidth = baseWidth * (float)(slotCount - 1) + activeWidth + kTopBarTabGap * (float)(slotCount - 1);
            totalWidth += kTopBarSelectedMargin * 2.0f * (float)selectedCount;
            float cursorX = (1920.0f - totalWidth) * 0.5f;

            for (int slotIndex = 0; slotIndex < slotCount; slotIndex++) {
                int tabIndex = ResolveSlotTab(slotIndex, slotCount);
                bool isActive = (tabIndex == currentTab);
                bool isFocused = hostApi->topFocus() && hostApi->topSelection() == tabIndex;
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

                const char *tintTabId = hostApi->tabIdAt(tabIndex);
                float tintRed = 0.5f;
                float tintGreen = 0.5f;
                float tintBlue = 0.5f;

                if (tintTabId && hostApi->submenuIsDynamicTab(tabIndex)) {
                    if (!hostApi->submenuTabColor(tabIndex, &tintRed, &tintGreen, &tintBlue)) {
                        tintRed = 0.5f;
                        tintGreen = 0.5f;
                        tintBlue = 0.5f;
                    }
                } else if (tintTabId) {
                    hostApi->themeTabAccent(tintTabId, &tintRed, &tintGreen, &tintBlue);
                }

                if (isFocused) {
                    Recolor(s_state.recolorProgress, &tintRed, &tintGreen, &tintBlue);
                } else if (!isActive) {
                    tintRed = iconRed;
                    tintGreen = iconGreen;
                    tintBlue = iconBlue;
                }

                if (hostApi->gfxTexValid(artSlot)) {
                    hostApi->gfxPushIcon(iconX, iconY, iconSize, iconSize, 0.0f, 0.0f, 0.0f, 0.0f, tintRed, tintGreen, tintBlue, 0.0f, artSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
                } else {
                    unsigned glyphCodepoint = TabGlyph(tabIndex);

                    if (!glyphCodepoint) {
                        continue;
                    }

                    char glyphText[4] = { 0 };
                    GlyphUTF8(glyphCodepoint, glyphText);
                    float glyphSize = iconSize * 0.62f;
                    float glyphWidth = hostApi->fontMeasure(glyphText, glyphSize);
                    hostApi->fontDraw(glyphText, iconX + (iconSize - glyphWidth) * 0.5f, iconY + iconSize * 0.5f + glyphSize * 0.35f, glyphSize, tintRed, tintGreen, tintBlue, 1.0f);
                }
            }
        }


        /// @brief Draw the builtin clock text.
        /// @param inkRed Ink red.
        /// @param inkGreen Ink green.
        /// @param inkBlue Ink blue.
        void DrawClockWidget(float inkRed, float inkGreen, float inkBlue)
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            char timeText[16];
            snprintf(timeText, sizeof(timeText), "%s", hostApi->clockTimeStr());
            char meridiemText[8];
            snprintf(meridiemText, sizeof(meridiemText), "%s", hostApi->clockAmPm());
            float timeWidth = hostApi->fontMeasure(timeText, 40.0f);
            float meridiemWidth = hostApi->fontMeasure(meridiemText, 26.0f);
            float clockX = 1694.0f - timeWidth - 8.0f - meridiemWidth;
            hostApi->fontDraw(timeText, clockX, 130.0f, 40.0f, inkRed, inkGreen, inkBlue, 1.0f);
            hostApi->fontDraw(meridiemText, clockX + timeWidth + 8.0f, 130.0f, 26.0f, inkRed, inkGreen, inkBlue, 1.0f);
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
            const MenuHostApi *hostApi = GetLayoutHost();
            int wifiSlot = hostApi->iconsHudSlot(kHudWifiNone);
            float wifiRed = inkRed * 0.5f;
            float wifiGreen = inkGreen * 0.5f;
            float wifiBlue = inkBlue * 0.5f;

            if (hostApi->clockPlane() > 0) {
                wifiSlot = hostApi->iconsHudSlot(kHudAirplane);
                wifiRed = iconRed;
                wifiGreen = iconGreen;
                wifiBlue = iconBlue;
            } else {
                int connectionType = hostApi->clockNetType();
                int signalBars = hostApi->clockWifi();
                bool isConnected = (connectionType == QEXT_NET_WIFI && signalBars >= 1 && signalBars <= 3) ||
                                   connectionType == QEXT_NET_ETHERNET;

                if (connectionType == QEXT_NET_ETHERNET) {
                    wifiSlot = hostApi->iconsHudSlot(kHudWifi0);
                } else if (connectionType == QEXT_NET_WIFI) {
                    int clampedSignal = signalBars;

                    if (clampedSignal < 0) {
                        clampedSignal = 0;
                    }

                    if (clampedSignal > 3) {
                        clampedSignal = 3;
                    }

                    wifiSlot = hostApi->iconsHudSlot(kHudWifi0 + (3 - clampedSignal));
                }

                if (isConnected) {
                    wifiRed = iconRed;
                    wifiGreen = iconGreen;
                    wifiBlue = iconBlue;
                }
            }

            if (hostApi->gfxTexValid(wifiSlot)) {
                hostApi->gfxPushIcon(1717.0f, 88.0f, 51.0f, 51.0f, 0.0f, 0.0f, 0.0f, 0.0f, wifiRed, wifiGreen, wifiBlue, 0.0f, wifiSlot, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            } else {
                char wifiGlyph[4] = { 0 };
                GlyphUTF8(GLYPH_WIFI, wifiGlyph);
                hostApi->fontDraw(wifiGlyph, 1717.0f, 130.0f, 44.0f, wifiRed, wifiGreen, wifiBlue, 1.0f);
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
            const MenuHostApi *hostApi = GetLayoutHost();
            unsigned batteryPercent = hostApi->clockBatt();

            if (batteryPercent > 100) {
                batteryPercent = 100;
            }

            float fillRatio = (float)batteryPercent / 100.0f;

            float fillRed = 0.0f;
            float fillGreen = 0.0f;
            float fillBlue = 0.0f;

            if (hostApi->clockCharging() > 0) {
                hostApi->themeColor("good", &fillRed, &fillGreen, &fillBlue);
            } else if (!hostApi->themeTryColor("battery_fill", &fillRed, &fillGreen, &fillBlue)) {
                fillRed = fillGreen = fillBlue = 1.0f;
            }

            hostApi->gfxPushPanel(1790.0f, 95.0f, 64.0f, 36.0f, 8.0f, inkRed, inkGreen, inkBlue, 1.0f);
            hostApi->gfxPushPanel(1795.0f, 100.0f, 54.0f, 26.0f, 5.0f, menuBarRed, menuBarGreen, menuBarBlue, 1.0f);

            if (fillRatio > 0.0f) {
                hostApi->gfxPushPanel(1796.0f, 101.0f, 52.0f * fillRatio, 24.0f, 4.0f, fillRed, fillGreen, fillBlue, 1.0f);
            }

            hostApi->gfxPushPanel(1854.0f, 104.0f, 8.0f, 18.0f, 3.0f, inkRed, inkGreen, inkBlue, 1.0f);
        }

        /// @brief Draw the clock, wifi, and battery cluster on the right.
        void DrawStatusCluster()
        {
            const MenuHostApi *hostApi = GetLayoutHost();
            float inkRed = 0.0f;
            float inkGreen = 0.0f;
            float inkBlue = 0.0f;
            hostApi->themeColor("ink", &inkRed, &inkGreen, &inkBlue);

            float menuBarRed = 0.0f;
            float menuBarGreen = 0.0f;
            float menuBarBlue = 0.0f;
            hostApi->themeColor("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);

            float iconRed = 0.0f;
            float iconGreen = 0.0f;
            float iconBlue = 0.0f;
            hostApi->themeColor("icon", &iconRed, &iconGreen, &iconBlue);

            if (!hostApi->widgetHas("clock")) {
                DrawClockWidget(inkRed, inkGreen, inkBlue);
            }

            if (!hostApi->widgetHas("wifi")) {
                DrawWifiWidget(inkRed, inkGreen, inkBlue, iconRed, iconGreen, iconBlue);
            }

            if (!hostApi->widgetHas("battery")) {
                DrawBatteryWidget(inkRed, inkGreen, inkBlue, menuBarRed, menuBarGreen, menuBarBlue);
            }

            GetLayoutHost()->widgetDrawAll();
        }

    } // namespace

    /// @brief Draw the top bar (avatars, tab icons, clock, status).
    /// @param layoutNode Layout node (currently unused, geometry is fixed).
    void Draw(const LayoutNode &layoutNode)
    {
        (void)layoutNode;
        const MenuHostApi *hostApi = GetLayoutHost();
        hostApi->gfxPushLayer((int)Gfx::GfxLayer::MenuBar);

        int focusedTabIndex = hostApi->topFocus() ? hostApi->topSelection() : -1;

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
        hostApi->themeColor("menubar", &menuBarRed, &menuBarGreen, &menuBarBlue);
        hostApi->gfxPushPanel(0.0f, 0.0f, 1920.0f, kTopBarHeight, 0.0f, menuBarRed, menuBarGreen, menuBarBlue, 1.0f);
        hostApi->gfxPopLayer();

        hostApi->gfxPushLayer((int)Gfx::GfxLayer::MenuBarItems);
        DrawAvatars();
        DrawTabStrip(hostApi->appMenu());
        DrawStatusCluster();
        hostApi->gfxPopLayer();
    }

    /// @brief Handle top-bar navigation input.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void Input(u64 buttonsDown, u64 buttonsHeld)
    {
        const MenuHostApi *hostApi = GetLayoutHost();

        switch (CurrentPhase()) {
            case TopBarPhase::Inactive:
                return;

            case TopBarPhase::Navigating:
                break;
        }

        int tabCount = hostApi->tabCount();

        if (tabCount <= 0) {
            return;
        }

        s_state.keyRepeat.Poll(buttonsDown, buttonsHeld, HidNpadButton_AnyLeft | HidNpadButton_AnyRight, DefaultRepeat());

        int currentSelection = hostApi->topSelection();
        int slotCount = hostApi->layoutTabCount();
        int currentSlot = -1;

        for (int slotIndex = 0; slotIndex < slotCount; slotIndex++) {
            if (hostApi->layoutTabAt(slotIndex) == currentSelection) {
                currentSlot = slotIndex;
                break;
            }
        }

        if (buttonsDown & HidNpadButton_AnyLeft) {
            if (currentSlot >= 0) {
                if (currentSlot > 0) {
                    hostApi->setTopSelection(hostApi->layoutTabAt(currentSlot - 1));
                    hostApi->sfxPlay((int)Sfx::Hover);
                }
            } else if (currentSelection > 0) {
                hostApi->setTopSelection(currentSelection - 1);
                hostApi->sfxPlay((int)Sfx::Hover);
            }
        }

        if (buttonsDown & HidNpadButton_AnyRight) {
            if (currentSlot >= 0) {
                if (currentSlot < slotCount - 1) {
                    hostApi->setTopSelection(hostApi->layoutTabAt(currentSlot + 1));
                    hostApi->sfxPlay((int)Sfx::Hover);
                }
            } else if (currentSelection < tabCount - 1) {
                hostApi->setTopSelection(currentSelection + 1);
                hostApi->sfxPlay((int)Sfx::Hover);
            }
        }

        if (buttonsDown & HidNpadButton_AnyDown) {
            hostApi->setTopFocus(0);
            hostApi->sfxPlay((int)Sfx::Hover);
        }

        if (buttonsDown & HidNpadButton_B) {
            hostApi->setTopFocus(0);
            hostApi->sfxPlay((int)Sfx::Back);
        }
    }

} /* namespace WTopBar */
