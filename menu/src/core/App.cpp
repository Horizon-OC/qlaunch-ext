/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "App.hpp"
#include "Gfx.hpp"
#include "Font.hpp"
#include "Icons.hpp"
#include "HudAssets.hpp"
#include "Theme.hpp"
#include "Layout.hpp"
#include "Clock.hpp"
#include "Sfx.hpp"
#include "PadIcon.hpp"
#include "Tabs.hpp"
#include "Submenus.hpp"
#include "DynamicWidgets.hpp"
#include "HostApi.hpp"
#include "../widgets/Widgets.hpp"
#include "Avatars.hpp"
#include "Qext.hpp"
#include "Log.hpp"
#include <string.h>

namespace App {

    namespace {

        /// @brief Whole menu state, single global.
        AppState s_state;

        /// @brief True once Boot() has initialized static storage.
        bool s_booted = false;

        /// @brief Icon sync cache to avoid re-syncing every frame.
        struct IconSyncCache {
            int selection;
            int rowCount;
        };

        static IconSyncCache s_iconSync = { -1, -1 };

        /// @brief Menu slide action state machine.
        enum class MenuSlideAction {
            Idle,
            StartLeft,
            StartRight,
        };

        /// @brief (Re)assign icon slots for all rows, focus first.
        void AssignIconSlots()
        {
            AppState &appState = s_state;
            u64 titleIds[kAppMaxTitles + kAppAppletCount];
            int iconSlots[kAppMaxTitles + kAppAppletCount];

            for (int rowIndex = 0; rowIndex < appState.rowCount; rowIndex++) {
                appState.rows[rowIndex].iconSlot = -1;
                titleIds[rowIndex] = appState.rows[rowIndex].isGame ? appState.rows[rowIndex].tid : 0;
                iconSlots[rowIndex] = -1;
            }

            Icons::Sync(titleIds, iconSlots, appState.rowCount, appState.sel);

            for (int rowIndex = 0; rowIndex < appState.rowCount; rowIndex++) {
                appState.rows[rowIndex].iconSlot = iconSlots[rowIndex];
            }

            s_iconSync.selection = appState.sel;
            s_iconSync.rowCount = appState.rowCount;
        }

        /// @brief Reload the title list from qext, preserving selection.
        void Reload()
        {
            static const char fallback[] = "Unknown title";
            AppState &appState = s_state;

            u64 keepTid = 0;

            if (appState.sel >= 0 && appState.sel < appState.rowCount) {
                keepTid = appState.rows[appState.sel].tid;
            }

            int n = qext_title_count();

            if (n < 0) {
                n = 0;
            }

            if (n > kAppMaxTitles) {
                n = kAppMaxTitles;
            }

            appState.rowCount = 0;

            for (int i = 0; i < n; i++) {
                AppRow *r = &appState.rows[appState.rowCount++];
                r->isGame = true;
                r->tid = qext_title_id(i);
                r->ejected = qext_title_ejected(i) > 0;
                r->applet = -1;
                r->iconSlot = -1;
                r->name[0] = 0;

                int len = qext_title_name(i, r->name, kAppNameMax);

                if (len < 0 || r->name[0] == 0) {
                    unsigned k = 0;

                    while (fallback[k] && k + 1 < kAppNameMax) {
                        r->name[k] = fallback[k];
                        k++;
                    }

                    r->name[k] = 0;
                } else {
                    r->name[kAppNameMax - 1] = 0;
                }
            }

            int want = -1;

            for (int i = 0; i < appState.rowCount; i++) {
                if (appState.rows[i].tid == keepTid) {
                    want = i;
                    break;
                }
            }

            if (want >= 0) {
                appState.sel = want;
            } else if (appState.sel >= appState.rowCount) {
                appState.sel = appState.rowCount > 0 ? appState.rowCount - 1 : 0;
            }

            if (appState.sel < 0) {
                appState.sel = 0;
            }

            if (appState.focus.topSel < 0) {
                appState.focus.topSel = 0;
            }

            if (appState.focus.topSel >= Tabs::Count() && Tabs::Count() > 0) {
                appState.focus.topSel = Tabs::Count() - 1;
            }
        }

        /// @brief Present one frame, either blacked out or via Layout.
        /// @param black When true, push a fullscreen black quad.
        void Present(bool black)
        {
            Gfx::ResetCounts();

            if (black) {
                Gfx::PushQuad(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            } else {
                Layout::Draw();
            }

            if (!Gfx::BeginFrame()) {
                return;
            }

            Gfx::DrawColor();
            Gfx::DrawPanelsBg();
            Gfx::DrawPanels();
            Gfx::DrawHighlights();
            Gfx::DrawIcons();
            Gfx::DrawOverlay();
            Gfx::DrawTextBuf(Font::Slot(kFontBufLatin), kFontBufLatin);
            Gfx::DrawTextBuf(Font::Slot(kFontBufKana), kFontBufKana);
            Gfx::DrawTextBuf(Font::Slot(kFontBufIcon), kFontBufIcon);
            Gfx::EndFrame();
        }

        /// @brief Step the top-bar tab switch with debounce.
        /// @param delta -1 for left, +1 for right.
        void TopSwitch(int delta)
        {
            AppState &appState = s_state;

            if (appState.focus.topDebounce > 0) {
                return;
            }

            appState.focus.topDebounce = 39;

            int count = Tabs::Count();

            if (count <= 0) {
                return;
            }

            int m = (appState.menu.cur + delta + count) % count;
            appState.focus.topSel = m;
            SwitchMenu(m);
            MenuJingle(m);
        }

        /// @brief Reset volatile selection state to home defaults.
        void ResetSelection()
        {
            AppState &appState = s_state;
            appState.sel = 0;
            appState.focus.top = false;
            appState.focus.topSel = 0;
            appState.menu.cur = 0;
            appState.menu.from = 0;
            appState.menu.t = 1.0f;
            appState.menu.dir = 1;
            appState.menu.phase = MenuPhase_Idle;
            appState.focus.topDebounce = 0;
            appState.home.sub = kHomeSubGames;
            appState.home.shiftY = 0.0f;
            appState.focus.bottom = false;
            appState.focus.bottomSel = 0;
            appState.launchBlack = false;
            appState.videoOsdAt = -100.0f;
            appState.frame = 0;
            appState.holdFrames = 0;
            WPrompt::Close();
        }

    } // namespace

    /// @brief Direct access to the consolidated state (prefer accessors).
    /// @return Mutable app state.
    AppState &State()
    {
        return s_state;
    }

    /// @brief Boot the menu: gfx, font, theme, layout, tabs, input.
    /// @param win Native window handle.
    /// @return True on success.
    bool Boot(NWindow *win)
    {
        Shutdown();

        int fbW = 1280, fbH = 720;
        qext_display_size(&fbW, &fbH);
        Log::Line("[menu] display %dx%d", fbW, fbH);

        if (fbW < 640 || fbH < 360 || fbW > 7680 || fbH > 4320) {
            fbW = 1280;
            fbH = 720;
        }

        if ((unsigned long long)fbW * (unsigned long long)fbH * 4ULL > (unsigned long long)1280 * 720 * 4) {
            Log::Line("[menu] Too much memory, downsizing framebuffer");
            fbW = 1280;
            fbH = 720;
        }

        if (!Gfx::Init(win, fbW, fbH)) {
            return false;
        }

        if (!Font::Init()) {
            Shutdown();
            return false;
        }

        if (!Theme::InitAuto()) {
            Shutdown();
            return false;
        }

        logging::LogLine("[menu] build %s %s", __DATE__, __TIME__);
        Icons::InitHud(HudAssets::kDefs, HudAssets::kCount);

        Tabs::RegisterBuiltin();
        Submenus::Boot(HostApi::GetHostApi());
        DynamicWidgets::Boot(HostApi::GetHostApi());

        if (!Layout::LoadAuto()) {
            Shutdown();
            return false;
        }

        Sfx::Init();
        Sfx::PlayByName("home_boot");

        AppState &appState = s_state;
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&appState.pad);
        appState.padReady = true;
        appState.tick = 0;

        Clock::Tick();
        Avatars::Refresh();
        ResetSelection();
        s_booted = true;
        RefreshTitles();
        return true;
    }

    /// @brief Shut down subsystems and mark input dead.
    void Shutdown()
    {
        s_state.padReady = false;
        Submenus::Shutdown();
        DynamicWidgets::Shutdown();
        Sfx::Shutdown();
        Icons::FreeAll();
        Font::Shutdown();
        Theme::Shutdown();
        Gfx::Destroy();
        s_booted = false;
    }

    /// @brief Refresh title cache and re-sync icons.
    void RefreshTitles()
    {
        qext_refresh_titles();
        Reload();
        AssignIconSlots();
    }

    /// @brief Clamp selection by delta, clearing top focus.
    /// @param delta Signed step.
    void MoveSel(int delta)
    {
        AppState &appState = s_state;

        if (appState.rowCount <= 0) {
            return;
        }

        appState.sel += delta;

        if (appState.sel < 0) {
            appState.sel = 0;
        }

        if (appState.sel >= appState.rowCount) {
            appState.sel = appState.rowCount - 1;
        }

        appState.focus.top = false;
    }

    /// @brief Present two black frames (used when launching).
    void Blackout()
    {
        Present(true);
        Present(true);
    }

    /// @brief Launch a game title with blackout and jingles.
    /// @param tid Title id to launch.
    void LaunchGame(u64 tid)
    {
        AppState &appState = s_state;
        appState.launchBlack = true;
        Blackout();

        if (R_FAILED(qext_launch_title(tid))) {
            appState.launchBlack = false;
            Sfx::Play(Sfx::Back);
        } else {
            Sfx::Play(Sfx::Click);
        }
    }

    /// @brief Activate whatever is focused (top bar, bottom bar, title).
    /// @param heldButtons Buttons currently held (forwarded to prompts).
    /// @return True when the A press was consumed.
    bool Activate(u64 heldButtons)
    {
        AppState &appState = s_state;

        if (Submenus::InputLockedTabIndex() >= 0) {
            return false;
        }

        if (appState.focus.top) {
            Layout::ActivateTop();
            return true;
        }

        if (appState.focus.bottom) {
            ActivateBottom();
            return true;
        }

        int homeTabIndex = Tabs::FindById("home");

        if (appState.menu.cur != homeTabIndex || appState.focus.bottom || appState.focus.top ||
            (appState.home.sub != kHomeSubGames && appState.home.sub != kHomeSubVgc)) {
            return false;
        }

        if (appState.sel < 0 || appState.sel >= appState.rowCount) {
            return false;
        }

        AppRow *selectedRow = &appState.rows[appState.sel];

        if (selectedRow->isGame) {
            if (selectedRow->ejected) {
                Sfx::Play(Sfx::Back);
                return true;
            }

            u64 suspendedTitleId = qext_suspended_title();

            if (suspendedTitleId != 0 && suspendedTitleId == selectedRow->tid && qext_game_running() &&
                !qext_game_has_foreground()) {
                appState.launchBlack = true;
                Blackout();

                if (R_FAILED(qext_resume_game())) {
                    appState.launchBlack = false;
                    qext_terminate_game();
                    Sfx::Play(Sfx::Back);
                } else {
                    Sfx::Play(Sfx::Click);
                }

                return true;
            } else if (!qext_game_has_foreground()) {
                if (qext_game_running() && suspendedTitleId != selectedRow->tid) {
                    WPrompt::OpenCloseGame(heldButtons, selectedRow->tid);
                    return true;
                }

                LaunchGame(selectedRow->tid);
                return true;
            }

            return false;
        }

        qext_launch_applet(selectedRow->applet);
        Sfx::Play(Sfx::Click);
        return true;
    }

    /// @brief Main per-frame pump: input, anim, present, audio.
    void Loop()
    {
        AppState &appState = s_state;

        if (!appState.padReady) {
            return;
        }

        padUpdate(&appState.pad);
        u64 buttonsDown = padGetButtonsDown(&appState.pad);
        u64 buttonsHeld = padGetButtons(&appState.pad);

        PadIcon::Tick();
        UpdateMenuAnim();
        UpdateHomeShift();

        if (appState.focus.topDebounce > 0) {
            appState.focus.topDebounce--;
        }

        if (WPrompt::IsOpen()) {
            WPrompt::Input(buttonsDown, buttonsHeld);
        } else if (Submenus::InputLockedTabIndex() >= 0 && appState.menu.t >= 1.0f) {
            Layout::Input(buttonsDown, buttonsHeld);
        } else if (appState.menu.t < 1.0f) {
            /* Lock input during transitions. */
        } else if (buttonsDown & HidNpadButton_L) {
            TopSwitch(-1);
        } else if (buttonsDown & HidNpadButton_R) {
            TopSwitch(1);
        } else {
            int homeTabIndex = Tabs::FindById("home");

            if (appState.menu.cur == homeTabIndex && !appState.focus.top) {
                if (buttonsDown & HidNpadButton_X) {
                    if (qext_game_running()) {
                        WPrompt::OpenCloseGame(buttonsHeld, 0);
                    } else {
                        Sfx::Play(Sfx::Back);
                    }
                }

                if (buttonsDown & HidNpadButton_Plus) {
                    RefreshTitles();
                }
            }

            if (buttonsDown & HidNpadButton_A) {
                if (Activate(buttonsHeld)) {
                    buttonsDown &= ~HidNpadButton_A;
                }
            }

            Layout::Input(buttonsDown, buttonsHeld);
        }

        /* Scrolling moves the icon budget; re-sync without reloading. */
        if (appState.sel != s_iconSync.selection || appState.rowCount != s_iconSync.rowCount)
            AssignIconSlots();

        appState.frame++;
        appState.tick++;

        if ((appState.frame % 60) == 0) {
            Clock::Tick();
        }

        if (appState.frame >= 150) {
            appState.frame = 0;
            RefreshTitles();
        }

        if (qext_titles_changed()) {
            RefreshTitles();
        }

        /* Upload any newly-decoded video frame before drawing. */
        Submenus::Tick();
        DynamicWidgets::Tick();

        Present(appState.launchBlack);
        Sfx::Tick();
    }

    /// @brief Handle foregrounding: clear launch black, refresh caches.
    void OnOpen()
    {
        AppState &appState = s_state;
        appState.launchBlack = false;
        Font::Refresh();
        RefreshTitles();
        Avatars::Refresh();
        Submenus::RefreshAll();
    }

    /// @brief Handle theme/content reload.
    void OnReload()
    {
        s_state.launchBlack = false;
        Font::Refresh();
        RefreshTitles();
        Submenus::RefreshAll();
    }

    /// @brief Handle HOME press: close viewers, snap back to home tab.
    void OnHome()
    {
        AppState &appState = s_state;

        WPrompt::Close();
        Submenus::CloseFullTabs();

        int homeTabIndex = Tabs::FindById("home");

        if (homeTabIndex < 0) {
            homeTabIndex = 0;
        }

        appState.menu.cur = homeTabIndex;
        appState.menu.from = homeTabIndex;
        appState.menu.t = 1.0f;
        appState.menu.phase = MenuPhase_Idle;
        appState.focus.topSel = homeTabIndex;
        appState.sel = 0;
        appState.focus.top = false;
        appState.home.sub = kHomeSubGames;
    }

    /// @brief Handle power button (currently a no-op hook).
    void OnPower()
    {
    }

    /// @brief Give the number of home rows.
    /// @return Row count.
    int RowCount()
    {
        return s_state.rowCount;
    }

    /// @brief Give a home row by index (clamped).
    /// @param i Row index.
    /// @return Row reference.
    const AppRow &Row(int i)
    {
        if (i < 0) {
            i = 0;
        }

        if (i >= s_state.rowCount && s_state.rowCount > 0) {
            i = s_state.rowCount - 1;
        }

        return s_state.rows[i < 0 ? 0 : i];
    }

    /// @brief Give the selected home row.
    /// @return Selection index.
    int Sel()
    {
        return s_state.sel;
    }

    /// @brief Set the selected home row.
    /// @param s New selection.
    void SetSel(int s)
    {
        s_state.sel = s;
    }

    /// @brief Whether the top bar has focus.
    /// @return True when focused.
    bool TopFocus()
    {
        return s_state.focus.top;
    }

    /// @brief Set top-bar focus.
    /// @param f New focus flag.
    void SetTopFocus(bool f)
    {
        s_state.focus.top = f;
    }

    /// @brief Give the top-bar selection (tab index).
    /// @return Tab index.
    int TopSel()
    {
        return s_state.focus.topSel;
    }

    /// @brief Set the top-bar selection, clamped to the tab registry.
    /// @param s New tab index.
    void SetTopSel(int s)
    {
        int count = Tabs::Count();

        if (s < 0) {
            s = 0;
        }

        if (count > 0 && s >= count) {
            s = count - 1;
        }

        s_state.focus.topSel = s;
    }

    /// @brief Give the current menu tab index.
    /// @return Tab index.
    int Menu()
    {
        return s_state.menu.cur;
    }

    /// @brief Give the tab we are transitioning from.
    /// @return Tab index.
    int MenuFrom()
    {
        return s_state.menu.from;
    }

    /// @brief Give the eased transition progress 0-1.
    /// @return Eased progress.
    float MenuT()
    {
        float t = s_state.menu.t;

        if (t < 0.0f) {
            t = 0.0f;
        }

        if (t > 1.0f) {
            t = 1.0f;
        }

        /* Tween.TRANS_CUBIC easeInOut like menus.gd. */
        if (t < 0.5f) {
            return 4.0f * t * t * t;
        }

        float u = -2.0f * t + 2.0f;
        return 1.0f - u * u * u / 2.0f;
    }

    /// @brief Give the slide direction (-1 or +1).
    /// @return Direction.
    int MenuDir()
    {
        return s_state.menu.dir;
    }

    /// @brief Give the transition phase state machine value.
    /// @return Idle or Animating.
    MenuPhase MenuPhaseOf()
    {
        return s_state.menu.phase;
    }

    /// @brief Whether a menu transition is animating.
    /// @return True when busy.
    bool MenuBusy()
    {
        return s_state.menu.phase == MenuPhase_Animating;
    }

    /// @brief Play the data-driven jingle for a tab index.
    /// @param m Tab index into the Tabs registry.
    void MenuJingle(int m)
    {
        const MenuTab *tab = Tabs::Get(m);

        if (!tab || !tab->jingle[0]) {
            return;
        }

        Sfx::PlayByName(tab->jingle);
    }

    /// @brief Play the data-driven jingle for a tab id.
    /// @param id Tab id (for example "album").
    void MenuJingleById(const char *id)
    {
        MenuJingle(Tabs::FindById(id));
    }

    /// @brief Switch to a tab index, starting the slide state machine.
    /// @param menuIndex Tab index.
    void SwitchMenu(int menuIndex)
    {
        AppState &appState = s_state;

        if (menuIndex < 0 || menuIndex >= Tabs::Count()) {
            return;
        }

        appState.focus.top = false;
        appState.focus.bottom = false;

        if (menuIndex == appState.menu.cur && appState.menu.t >= 1.0f) {
            if (Submenus::IsDynamicTab(menuIndex)) {
                Submenus::EnterDynamicTab(menuIndex);
            } else {
                const MenuTab *sameTab = Tabs::Get(menuIndex);

                if (sameTab && sameTab->onEnter) {
                    sameTab->onEnter();
                }
            }

            return;
        }

        if (Submenus::IsDynamicTab(appState.menu.cur) && appState.menu.cur != menuIndex) {
            Submenus::ExitDynamicTab(appState.menu.cur);
        }

        appState.menu.from = appState.menu.cur;
        appState.menu.cur = menuIndex;
        appState.menu.dir = (appState.menu.cur > appState.menu.from) ? 1 : -1;

        if (appState.menu.cur == appState.menu.from) {
            appState.menu.dir = 1;
        }

        appState.menu.t = 0.0f;
        appState.menu.phase = MenuPhase_Animating;
        appState.focus.topSel = menuIndex;
        appState.focus.top = false;

        if (Submenus::IsDynamicTab(menuIndex)) {
            Submenus::EnterDynamicTab(menuIndex);
        } else {
            const MenuTab *nextTab = Tabs::Get(menuIndex);

            if (nextTab && nextTab->onEnter) {
                nextTab->onEnter();
            }
        }
    }

    /// @brief Switch to a tab by string id.
    /// @param id Tab id.
    void SwitchMenuById(const char *id)
    {
        SwitchMenu(Tabs::FindById(id));
    }

    /// @brief Tick the menu slide state machine.
    void UpdateMenuAnim()
    {
        AppState &appState = s_state;

        if (appState.menu.phase == MenuPhase_Idle) {
            return;
        }

        appState.menu.t += 1.0f / 15.0f;

        if (appState.menu.t >= 1.0f) {
            appState.menu.t = 1.0f;
            appState.menu.phase = MenuPhase_Idle;

            if (!Submenus::IsFullTab(appState.menu.cur)) {
                Submenus::CloseFullTabs();
            }
        }
    }

    /// @brief Give the home sub-screen.
    /// @return HOMESUB_* value.
    int HomeSub()
    {
        return s_state.home.sub;
    }

    /// @brief Set the home sub-screen, clamped.
    /// @param s New sub-screen.
    void SetHomeSub(int s)
    {
        if (s < 0) {
            s = 0;
        }

        if (s > 2) {
            s = 2;
        }

        s_state.home.sub = s;
    }

    /// @brief Give the vertical shift for home sub-screens.
    /// @return Shift in pixels.
    float HomeShiftY()
    {
        return s_state.home.shiftY;
    }

    /// @brief Whether the home shift animation is still settling.
    /// @return True when busy.
    bool HomeShiftBusy()
    {
        float target = 0.0f;

        if (s_state.home.sub == kHomeSubFolders) {
            target = -830.0f;
        } else if (s_state.home.sub == kHomeSubVgc) {
            target = 935.0f;
        }

        float d = target - s_state.home.shiftY;
        return d <= -1.0f || d >= 1.0f;
    }

    /// @brief Tick the home shift spring toward its target.
    void UpdateHomeShift()
    {
        float target = 0.0f;

        if (s_state.home.sub == kHomeSubFolders) {
            target = -830.0f;
        } else if (s_state.home.sub == kHomeSubVgc) {
            target = 935.0f;
        }

        float d = target - s_state.home.shiftY;

        if (d > -1.0f && d < 1.0f) {
            s_state.home.shiftY = target;
        } else {
            s_state.home.shiftY += d * 0.12f;
        }
    }

    /// @brief Whether the bottom bar has focus.
    /// @return True when focused.
    bool BottomFocus()
    {
        return s_state.focus.bottom;
    }

    /// @brief Set bottom-bar focus.
    /// @param f New focus flag.
    void SetBottomFocus(bool f)
    {
        s_state.focus.bottom = f;
    }

    /// @brief Give the bottom-bar selection.
    /// @return Index 0-2.
    int BottomSel()
    {
        return s_state.focus.bottomSel;
    }

    /// @brief Set the bottom-bar selection, clamped 0-2.
    /// @param s New selection.
    void SetBottomSel(int s)
    {
        if (s < 0) {
            s = 0;
        }

        if (s > 2) {
            s = 2;
        }

        s_state.focus.bottomSel = s;
    }

    /// @brief Activate the focused bottom-bar icon.
    void ActivateBottom()
    {
        AppState &appState = s_state;

        if (HomeShiftBusy()) {
            return;
        }

        /* Bottom-middle icons: folders / game cards toggle their Home
           sub-screen, sleep sleeps the console. */
        if (BottomSel() == 2) {
            qext_sleep();
            return;
        }

        int want = BottomSel() == 0 ? kHomeSubFolders : kHomeSubVgc;

        if (BottomSel() == 0) {
            Sfx::PlayByName("folder_jingle");
        } else {
            Sfx::PlayByName("vgc_jingle");
        }

        SetHomeSub(want == HomeSub() ? kHomeSubGames : want);
        SetBottomFocus(false);
    }

    /// @brief Whether we blacked out for a launch.
    /// @return True when black.
    bool LaunchBlack()
    {
        return s_state.launchBlack;
    }

    /// @brief Give menu time in seconds (60Hz ticks).
    /// @return Seconds.
    float Time()
    {
        return (float)s_state.tick / 60.0f;
    }

    /// @brief Give the menu time the video OSD was last shown.
    /// @return Seconds (Time base).
    float VideoOsdAt()
    {
        return s_state.videoOsdAt;
    }

    /// @brief Stamp the video OSD timer to now.
    void TouchVideoOsd()
    {
        s_state.videoOsdAt = Time();
    }

} /* namespace App */
