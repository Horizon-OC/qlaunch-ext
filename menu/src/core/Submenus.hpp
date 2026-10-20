#pragma once
#include "MenuHost.hpp"

namespace Submenus {

    /// @brief Boot dynamic submenus from a theme pack directory.
    /// @param hostApi Host table with menu functions plus qext.
    /// @param submenuDir Pack submenus dir (for example "sdmc:/qlaunch-ext/NekoMenu/default/submenus").
    void Boot(const MenuHostApi *hostApi, const char *submenuDir);

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

    /// @brief Act on a registered touch cell (select, re-tap acts).
    /// @param tabIndex Tabs registry index.
    /// @param cellIndex Cell index from touchRegisterCell.
    /// @return Nonzero when the tap was consumed.
    int TouchAction(int tabIndex, int cellIndex);

    /// @brief Handle input for a dynamic submenu tab.
    /// @param tabIndex Tabs registry index.
    /// @param buttonsDown Buttons pressed.
    /// @param buttonsHeld Buttons held.
    void InputDynamicTab(int tabIndex, unsigned long long buttonsDown, unsigned long long buttonsHeld);

    /// @brief Drag-scroll submenu content with inertia on release.
    /// @param tabIndex Tabs registry index.
    /// @param moveX Horizontal finger travel this frame.
    /// @param moveY Vertical finger travel this frame.
    /// @param phase Touch phase (held moves, up releases).
    /// @return Nonzero when the drag was consumed.
    int Drag(int tabIndex, float moveX, float moveY, int phase);

    /// @brief Deliver one tap to a submenu tab (select, double-tap runs).
    /// @param tabIndex Tabs registry index.
    /// @param x Tap X in design space.
    /// @param y Tap Y in design space.
    /// @return Nonzero when the tap was consumed.
    int Tap(int tabIndex, float x, float y);

    /// @brief Deliver a finger-down press to a submenu tab.
    /// @param tabIndex Tabs registry index.
    /// @param x Down X in design space.
    /// @param y Down Y in design space.
    /// @return Nonzero when the submenu claimed the finger.
    int TouchDown(int tabIndex, float x, float y);

    /// @brief Deliver a claimed finger release to a submenu tab.
    /// @param tabIndex Tabs registry index.
    /// @param x Up X in design space.
    /// @param y Up Y in design space.
    /// @return Nonzero when the release was consumed.
    int TouchUp(int tabIndex, float x, float y);

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
