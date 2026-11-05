#pragma once
#include "MenuHost.hpp"

namespace MenuPlugins {

    /// @brief Boot menu plugins from a pack plugins directory.
    /// @param hostApi Host table with menu functions plus qext.
    /// @param pluginDir Pack plugins dir (for example "sdmc:/qlaunch-ext/NekoMenu/default/plugins").
    void Boot(const MenuHostApi *hostApi, const char *pluginDir);

    /// @brief Shut down menu plugins, freeing icons and unloading dnro files.
    void Shutdown();

    /// @brief Give the total registered button count.
    /// @return Button count across all loaded plugins.
    int ButtonCount();

    /// @brief Give a button's tab registry index.
    /// @param button Global button index.
    /// @return Tabs registry index, or -1 when the tab is unknown.
    int ButtonTab(int button);

    /// @brief Give a button's label.
    /// @param button Global button index.
    /// @return Label, or "" when missing.
    const char *ButtonLabel(int button);

    /// @brief Give a button's icon texture slot.
    /// @param button Global button index.
    /// @return Texture slot, or -1 when the button has no icon.
    int ButtonIconSlot(int button);

    /// @brief Give the button count for one tab.
    /// @param tabIndex Tabs registry index.
    /// @return Button count targeting that tab.
    int ButtonCountForTab(int tabIndex);

    /// @brief Map a tab-local button slot to the global button index.
    /// @param tabIndex Tabs registry index.
    /// @param slot Tab-local slot (0..ButtonCountForTab-1).
    /// @return Global button index, or -1 when out of range.
    int ButtonGlobalAt(int tabIndex, int slot);

    /// @brief Activate a button through its plugin.
    /// @param button Global button index.
    /// @return Nonzero when the press was consumed.
    int Activate(int button);

} /* namespace MenuPlugins */
