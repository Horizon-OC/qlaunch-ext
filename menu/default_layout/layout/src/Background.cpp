/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "MenuHost.hpp"
#include "LayoutHost.hpp"

namespace WBackground {

    /// @brief Draw the fullscreen background for the active (or outgoing) tab.
    /// @param layoutNode Layout node (reserved for JSON-driven colors).
    void Draw(const LayoutNode &layoutNode)
    {
        (void)layoutNode;
        const MenuHostApi *hostApi = GetLayoutHost();

        int backgroundTabIndex = hostApi->menuT() < 1.0f ? hostApi->menuFrom() : hostApi->appMenu();
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
