/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */
#pragma once
#include <switch.h>
#include "Theme.hpp"
#include "MenuHost.hpp"

constexpr int kAppMaxTitles = 256;
constexpr int kAppAppletCount = 2;
constexpr int kFolderStripMax = 8;
constexpr int kFolderStripGames = 24;
constexpr int kFolderColorCount = 10;

constexpr int kHomeSubGames = 0;
constexpr int kHomeSubFolders = 1;

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
    u64 openFolder;  /* folder id shown in the folders view, 0 when none */
    int folderSel;   /* selection inside the open folder */
};

/// @brief Whole menu runtime state. Single global instead of ~20 loose ones.
struct AppState {
    AppRow rows[kAppMaxTitles + kAppAppletCount];
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
    float videoOsdAt; /* menu time the video OSD was last shown */
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
    /// @param buttonsHeld Buttons currently held (forwarded to prompts).
    /// @return True when the A press was consumed.
    bool Activate(u64 heldButtons);

    /// @brief Step the top-bar tab switch with debounce.
    /// @param delta -1 for left, +1 for right.
    void TopSwitch(int delta);

    /// @brief Launch a game title with blackout and jingles.
    /// @param tid Title id to launch.
    void LaunchGame(u64 tid);

    /// @brief Launch a game row with suspend/resume handling.
    /// @param tid Title id to launch.
    /// @param ejected True when the gamecard is ejected.
    /// @param heldButtons Buttons currently held (forwarded to prompts).
    /// @return True when the A press was consumed.
    bool LaunchGameRow(u64 tid, bool ejected, u64 heldButtons);

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

    /// @brief Give the open folder id.
    /// @return Folder id, or 0 when no folder view is open.
    u64 OpenFolder();

    /// @brief Set the open folder id.
    /// @param folderId Folder id, or 0 to close the view.
    void SetOpenFolder(u64 folderId);

    /// @brief Give the selection inside the open folder.
    /// @return Member index.
    int FolderSel();

    /// @brief Set the selection inside the open folder.
    /// @param s New member index.
    void SetFolderSel(int s);

    /// @brief Open a folder view on the strip.
    /// @param folderId Folder id to show.
    void OpenFolderView(u64 folderId);

    /// @brief Leave the folder view for the strip.
    void CloseFolderView();

    /// @brief Select the strip row holding a folder.
    /// @param folderId Folder id to select.
    void SelectFolderRow(u64 folderId);

    /// @brief Whether the bottom bar has focus.
    /// @return True when focused.
    bool BottomFocus();

    /// @brief Set bottom-bar focus.
    /// @param f New focus flag.
    void SetBottomFocus(bool f);

    /// @brief Give the bottom-bar selection.
    /// @return Index 0-1.
    int BottomSel();

    /// @brief Set the bottom-bar selection, clamped 0-1.
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

    /// @brief Give menu time in seconds (60Hz ticks).
    /// @return Seconds.
    float Time();

    /// @brief Give the menu time the video OSD was last shown.
    /// @return Seconds (Time base).
    float VideoOsdAt();

    /// @brief Stamp the video OSD timer to now.
    void TouchVideoOsd();

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
