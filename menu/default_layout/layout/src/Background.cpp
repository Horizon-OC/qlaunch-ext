/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "MenuHost.hpp"
#include "LayoutHost.hpp"
#include "App.hpp"

namespace WBackground {

    /// @brief Draw the fullscreen background for the active tab, or side-by-side
    ///        tab backgrounds during transitions (sharp cutoff, scrolled).
    /// @param layoutNode Layout node (reserved for JSON-driven colors).
    void Draw(const LayoutNode &layoutNode)
    {
        (void)layoutNode;
        const MenuHostApi *hostApi = GetLayoutHost();

        if (hostApi->menuPhase() == MenuPhase_Animating && hostApi->menuT() < 1.0f) {
            float outgoingX = hostApi->menuSlideX();
            float incomingX = outgoingX + (float)hostApi->menuDir() * 1920.0f;

            int fromTab = hostApi->menuFrom();
            int toTab = hostApi->appMenu();
            const char *fromId = hostApi->tabIdAt(fromTab);
            const char *toId = hostApi->tabIdAt(toTab);

            float fr = 1.0f, fg = 1.0f, fb = 1.0f;
            float tr = 1.0f, tg = 1.0f, tb = 1.0f;
            if (fromId) hostApi->themeTabColor(fromId, &fr, &fg, &fb);
            if (toId)   hostApi->themeTabColor(toId, &tr, &tg, &tb);

            /* Side-by-side backgrounds scroll with the tabs (sharp cutoff). */
            hostApi->gfxPushPanel(outgoingX, 0.0f, 1920.0f, 1080.0f, 0.0f, fr, fg, fb, 1.0f);
            hostApi->gfxPushPanel(incomingX, 0.0f, 1920.0f, 1080.0f, 0.0f, tr, tg, tb, 1.0f);
            return;
        }

        int backgroundTabIndex = hostApi->appMenu();
        const char *backgroundTabId = hostApi->tabIdAt(backgroundTabIndex);

        float backgroundRed = 1.0f;
        float backgroundGreen = 1.0f;
        float backgroundBlue = 1.0f;

        if (backgroundTabId) {
            hostApi->themeTabColor(backgroundTabId, &backgroundRed, &backgroundGreen, &backgroundBlue);
        }

        hostApi->gfxPushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, backgroundRed, backgroundGreen, backgroundBlue, 1.0f);
    }

} /* namespace WBackground */
