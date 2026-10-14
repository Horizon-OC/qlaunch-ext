/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */
#pragma once
#include <switch.h>
#include "Theme.hpp"

constexpr int APP_MAX_TITLES = 256;
constexpr int APP_NAME_MAX = 192;
constexpr int APP_APPLETS = 2;

constexpr int HOMESUB_GAMES = 0;
constexpr int HOMESUB_FOLDERS = 1;
constexpr int HOMESUB_VGC = 2;

/// @brief One row in the home game strip.
struct AppRow {
    bool isGame;
    u64 tid;
    int applet; /* 0 album, 1 controllers */
    int iconSlot;
    bool ejected;
    char name[APP_NAME_MAX];
};

/// @brief Phase of the menu slide transition.
enum MenuPhase {
    MenuPhase_Idle = 0,
    MenuPhase_Animating = 1,
};

/// @brief Slide transition between two tabs.
struct MenuTransition {
    int cur;
    int from;
    float t;   /* Linear 0-1 progress. */
    int dir;   /* -1 or +1 slide direction. */
    MenuPhase phase;
};

/// @brief Focus + selection state. Replaces loose top/bottom globals.
struct FocusState {
    bool top;
    bool bottom;
    int topSel;
    int bottomSel;
    unsigned topDebounce;
};

/// @brief Home strip + sub-screen state.
struct HomeState {
    int sub;
    float shiftY;
    int settingsPage;
    int settingsRow;
    bool eshopNews;
    int friendSel;
};

/// @brief Whole menu runtime state. Single global instead of ~20 loose ones.
struct AppState {
    AppRow rows[APP_MAX_TITLES + APP_APPLETS];
    int rowCount;
    int sel;
    MenuTransition menu;
    FocusState focus;
    HomeState home;
    bool launchBlack;
    PadState pad;
    bool padReady;
    unsigned frame;
    unsigned holdFrames;
    unsigned tick;
};

namespace App {
    /// @brief Boot the menu: gfx, font, theme, layout, tabs, input.
    /// @param win Native window handle.
    /// @return True on success.
    bool Boot(NWindow *win);

    /// @brief Main per-frame pump: input, anim, present, audio.
    void Loop();

    /// @brief Shut down subsystems and mark input dead.
    void Shutdown();

    /// @brief Handle foregrounding: clear launch black, refresh caches.
    void OnOpen();

    /// @brief Handle theme/content reload.
    void OnReload();

    /// @brief Handle HOME press: close album, snap back to home tab.
    void OnHome();

    /// @brief Handle power button (currently a no-op hook).
    void OnPower();

    /// @brief Refresh title cache and re-sync icons.
    void RefreshTitles();

    /// @brief Activate whatever is focused (top bar, bottom bar, title).
    /// @return True when the A press was consumed.
    bool Activate();

    /// @brief Present two black frames (used when launching).
    void Blackout();

    /// @brief Give the number of home rows.
    /// @return Row count.
    int RowCount();

    /// @brief Give a home row by index (clamped).
    /// @param i Row index.
    /// @return Row reference.
    const AppRow &Row(int i);

    /// @brief Give the selected home row.
    /// @return Selection index.
    int Sel();

    /// @brief Set the selected home row.
    /// @param s New selection.
    void SetSel(int s);

    /// @brief Whether the top bar has focus.
    /// @return True when focused.
    bool TopFocus();

    /// @brief Set top-bar focus.
    /// @param f New focus flag.
    void SetTopFocus(bool f);

    /// @brief Give the top-bar selection (tab index).
    /// @return Tab index.
    int TopSel();

    /// @brief Set the top-bar selection, clamped to the tab registry.
    /// @param s New tab index.
    void SetTopSel(int s);

    /// @brief Give the current menu tab index.
    /// @return Tab index.
    int Menu();

    /// @brief Give the tab we are transitioning from.
    /// @return Tab index.
    int MenuFrom();

    /// @brief Give the eased transition progress 0-1.
    /// @return Eased progress.
    float MenuT();

    /// @brief Give the slide direction (-1 or +1).
    /// @return Direction.
    int MenuDir();

    /// @brief Give the transition phase state machine value.
    /// @return Idle or Animating.
    MenuPhase MenuPhaseOf();

    /// @brief Whether a menu transition is animating.
    /// @return True when busy.
    bool MenuBusy();

    /// @brief Switch to a tab index, starting the slide state machine.
    /// @param m Tab index.
    void SwitchMenu(int m);

    /// @brief Switch to a tab by string id.
    /// @param id Tab id.
    void SwitchMenuById(const char *id);

    /// @brief Play the data-driven jingle for a tab index.
    /// @param m Tab index into the Tabs registry.
    void MenuJingle(int m);

    /// @brief Play the data-driven jingle for a tab id.
    /// @param id Tab id (for example "album").
    void MenuJingleById(const char *id);

    /// @brief Tick the menu slide state machine.
    void UpdateMenuAnim();

    /// @brief Give the home sub-screen.
    /// @return HOMESUB_* value.
    int HomeSub();

    /// @brief Set the home sub-screen, clamped.
    /// @param s New sub-screen.
    void SetHomeSub(int s);

    /// @brief Whether the bottom bar has focus.
    /// @return True when focused.
    bool BottomFocus();

    /// @brief Set bottom-bar focus.
    /// @param f New focus flag.
    void SetBottomFocus(bool f);

    /// @brief Give the bottom-bar selection.
    /// @return Index 0-2.
    int BottomSel();

    /// @brief Set the bottom-bar selection, clamped 0-2.
    /// @param s New selection.
    void SetBottomSel(int s);

    /// @brief Activate the focused bottom-bar icon.
    void ActivateBottom();

    /// @brief Give the vertical shift for home sub-screens.
    /// @return Shift in pixels.
    float HomeShiftY();

    /// @brief Whether the home shift animation is still settling.
    /// @return True when busy.
    bool HomeShiftBusy();

    /// @brief Tick the home shift spring toward its target.
    void UpdateHomeShift();

    /// @brief Give the settings page (-1 = option list).
    /// @return Page index.
    int SettingsPage();

    /// @brief Set the settings page and reset the row cursor.
    /// @param p New page.
    void SetSettingsPage(int p);

    /// @brief Give the settings row cursor.
    /// @return Row index.
    int SettingsRow();

    /// @brief Set the settings row cursor.
    /// @param r New row.
    void SetSettingsRow(int r);

    /// @brief Whether the shop shows news instead of the grid.
    /// @return True for news.
    bool EShopNews();

    /// @brief Set the shop news flag.
    /// @param n New flag.
    void SetEShopNews(bool n);

    /// @brief Give the friends selection cursor.
    /// @return Index.
    int FriendSel();

    /// @brief Set the friends selection cursor.
    /// @param s New index.
    void SetFriendSel(int s);

    /// @brief Give menu time in seconds (60Hz ticks).
    /// @return Seconds.
    float Time();

    /// @brief Clamp selection by delta, clearing top focus.
    /// @param delta Signed step.
    void MoveSel(int delta);

    /// @brief Whether we blacked out for a launch.
    /// @return True when black.
    bool LaunchBlack();

    /// @brief Direct access to the consolidated state (prefer accessors).
    /// @return Mutable app state.
    AppState &State();
} /* namespace App */
