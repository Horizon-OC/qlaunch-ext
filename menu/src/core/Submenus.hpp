#pragma once
#include "MenuHost.hpp"

namespace Submenus {

    /// @brief Boot dynamic submenus from the NekoMenu directory.
    /// @param hostApi Host table with menu functions plus qext.
    void Boot(const MenuHostApi *hostApi);

    /// @brief Shut down submenus, freeing icons and unloading dnro files.
    void Shutdown();

    /// @brief Tick loaded submenus once per frame.
    void Tick();

    /// @brief Whether a tab index belongs to a dynamic submenu.
    /// @param tabIndex Tabs registry index.
    /// @return True for dynamic submenu tabs.
    bool IsDynamicTab(int tabIndex);

    /// @brief Draw a dynamic submenu tab onto the core state.
    /// @param tabIndex Tabs registry index.
    void DrawDynamicTab(int tabIndex);

    /// @brief Handle input for a dynamic submenu tab.
    /// @param tabIndex Tabs registry index.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void InputDynamicTab(int tabIndex, unsigned long long buttonsDown, unsigned long long buttonsHeld);

    /// @brief Enter a dynamic submenu tab.
    /// @param tabIndex Tabs registry index.
    void EnterDynamicTab(int tabIndex);

    /// @brief Exit a dynamic submenu tab.
    /// @param tabIndex Tabs registry index.
    void ExitDynamicTab(int tabIndex);

    /// @brief Give a dynamic submenu tab accent color.
    /// @param tabIndex Tabs registry index.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return True when the submenu provides a color.
    bool TabColor(int tabIndex, float *redOut, float *greenOut, float *blueOut);

    /// @brief Give a dynamic submenu tab background color.
    /// @param tabIndex Tabs registry index.
    /// @param redOut Red output.
    /// @param greenOut Green output.
    /// @param blueOut Blue output.
    /// @return True when the submenu provides a color.
    bool TabBackgroundColor(int tabIndex, float *redOut, float *greenOut, float *blueOut);

    /// @brief Whether a dynamic submenu tab covers the screen.
    /// @param tabIndex Tabs registry index.
    /// @return True when fullscreen photo or video is showing.
    bool IsFullTab(int tabIndex);

    /// @brief Find the covering tab, if any.
    /// @return Tabs registry index covering the screen, or -1.
    int FullTabIndex();

    /// @brief Whether any covering tab is showing.
    /// @return True when a fullscreen photo or video is showing.
    bool AnyFullOpen();

    /// @brief Close every covering viewer.
    void CloseFullTabs();

    /// @brief Find the tab that wants all input.
    /// @return Tabs registry index wanting input lock, or -1.
    int InputLockedTabIndex();

    /// @brief Drop cached textures in every tab after an external refresh.
    void RefreshAll();

} /* namespace Submenus */
