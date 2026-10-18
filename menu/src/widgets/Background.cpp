/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Theme.hpp"
#include "../core/App.hpp"
#include "../core/Tabs.hpp"

namespace WBackground {

    /// @brief Draw the fullscreen background for the active (or outgoing) tab.
    /// @param layoutNode Layout node (reserved for JSON-driven colors).
    void Draw(const LayoutNode &layoutNode)
    {
        (void)layoutNode;

        int backgroundTabIndex = App::MenuT() < 1.0f ? App::MenuFrom() : App::Menu();
        const MenuTab *backgroundTab = Tabs::Get(backgroundTabIndex);
        int backgroundSlot = backgroundTab ? Theme::MenuSlotForId(backgroundTab->id) : -1;

        float backgroundRed = 0.0f;
        float backgroundGreen = 0.0f;
        float backgroundBlue = 0.0f;
        Theme::MenuColor(backgroundSlot, &backgroundRed, &backgroundGreen, &backgroundBlue);

        Gfx::PushPanel(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, backgroundRed, backgroundGreen, backgroundBlue, 1.0f);
    }

} /* namespace WBackground */
