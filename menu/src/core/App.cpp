/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "App.hpp"
#include "Gfx.hpp"
#include "Font.hpp"
#include "Icons.hpp"
#include "Theme.hpp"
#include "Layout.hpp"
#include "Clock.hpp"
#include "Sfx.hpp"
#include "PadIcon.hpp"
#include "Tabs.hpp"
#include "../widgets/Widgets.hpp"
#include "Avatars.hpp"
#include "Qext.hpp"
#include "Log.hpp"
#include <string.h>

namespace App {

    namespace {

        /// @brief State
        AppState s_state;

        /// @brief True once Boot() has initialized static storage.
        bool s_booted = false;

        /// @brief Selection and row count the icon slots were last synced for.
        int s_iconSel = -1;
        int s_iconRows = -1;

        /// @brief (Re)assign icon slots for all rows, focus first.
        void AssignIconSlots()
        {
            AppState &st = s_state;
            u64 tids[APP_MAX_TITLES + APP_APPLETS];
            int slots[APP_MAX_TITLES + APP_APPLETS];

            for (int i = 0; i < st.rowCount; i++) {
                st.rows[i].iconSlot = -1;
                tids[i] = st.rows[i].isGame ? st.rows[i].tid : 0;
                slots[i] = -1;
            }

            Icons::Sync(tids, slots, st.rowCount, st.sel);

            for (int i = 0; i < st.rowCount; i++) {
                st.rows[i].iconSlot = slots[i];
            }

            s_iconSel = st.sel;
            s_iconRows = st.rowCount;
        }

        /// @brief Reload the title list from qext, preserving selection.
        void Reload()
        {
            static const char fallback[] = "Unknown title";
            AppState &st = s_state;

            u64 keepTid = 0;

            if (st.sel >= 0 && st.sel < st.rowCount) {
                keepTid = st.rows[st.sel].tid;
            }

            int n = qext_title_count();

            if (n < 0) {
                n = 0;
            }

            if (n > APP_MAX_TITLES) {
                n = APP_MAX_TITLES;
            }

            st.rowCount = 0;

            for (int i = 0; i < n; i++) {
                AppRow *r = &st.rows[st.rowCount++];
                r->isGame = true;
                r->tid = qext_title_id(i);
                r->ejected = qext_title_ejected(i) > 0;
                r->applet = -1;
                r->iconSlot = -1;
                r->name[0] = 0;

                int len = qext_title_name(i, r->name, APP_NAME_MAX);

                if (len < 0 || r->name[0] == 0) {
                    unsigned k = 0;

                    while (fallback[k] && k + 1 < APP_NAME_MAX) {
                        r->name[k] = fallback[k];
                        k++;
                    }

                    r->name[k] = 0;
                } else {
                    r->name[APP_NAME_MAX - 1] = 0;
                }
            }

            int want = -1;

            for (int i = 0; i < st.rowCount; i++) {
                if (st.rows[i].tid == keepTid) {
                    want = i;
                    break;
                }
            }

            if (want >= 0) {
                st.sel = want;
            } else if (st.sel >= st.rowCount) {
                st.sel = st.rowCount > 0 ? st.rowCount - 1 : 0;
            }

            if (st.sel < 0) {
                st.sel = 0;
            }

            if (st.focus.topSel < 0) {
                st.focus.topSel = 0;
            }

            if (st.focus.topSel >= Tabs::Count() && Tabs::Count() > 0) {
                st.focus.topSel = Tabs::Count() - 1;
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
            Gfx::DrawTextBuf(Font::Slot(FONT_BUF_LATIN), FONT_BUF_LATIN);
            Gfx::DrawTextBuf(Font::Slot(FONT_BUF_KANA), FONT_BUF_KANA);
            Gfx::DrawTextBuf(Font::Slot(FONT_BUF_ICON), FONT_BUF_ICON);
            Gfx::EndFrame();
        }

        /// @brief Step the top-bar tab switch with debounce.
        /// @param delta -1 for left, +1 for right.
        void TopSwitch(int delta)
        {
            AppState &st = s_state;

            if (st.focus.topDebounce > 0) {
                return;
            }

            st.focus.topDebounce = 39;

            int count = Tabs::Count();

            if (count <= 0) {
                return;
            }

            int m = (st.menu.cur + delta + count) % count;
            st.focus.topSel = m;
            SwitchMenu(m);
            MenuJingle(m);
        }

        /// @brief Reset volatile selection state to home defaults.
        void ResetSelection()
        {
            AppState &st = s_state;
            st.sel = 0;
            st.focus.top = false;
            st.focus.topSel = 0;
            st.menu.cur = 0;
            st.menu.from = 0;
            st.menu.t = 1.0f;
            st.menu.dir = 1;
            st.menu.phase = MenuPhase_Idle;
            st.focus.topDebounce = 0;
            st.home.sub = HOMESUB_GAMES;
            st.home.shiftY = 0.0f;
            st.focus.bottom = false;
            st.focus.bottomSel = 0;
            st.home.settingsPage = -1;
            st.home.settingsRow = 0;
            st.home.eshopNews = false;
            st.home.friendSel = 0;
            st.launchBlack = false;
            st.frame = 0;
            st.holdFrames = 0;
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
        Icons::InitHud();

        Tabs::RegisterBuiltin();

        if (!Layout::LoadAuto()) {
            Shutdown();
            return false;
        }

        Sfx::Init();
        Sfx::PlayByName("home_boot");

        AppState &st = s_state;
        padConfigureInput(1, HidNpadStyleSet_NpadStandard);
        padInitializeDefault(&st.pad);
        st.padReady = true;
        st.tick = 0;

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
        Sfx::Shutdown();
        Icons::FreeAll();
        Font::Shutdown();
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
        AppState &st = s_state;

        if (st.rowCount <= 0) {
            return;
        }

        st.sel += delta;

        if (st.sel < 0) {
            st.sel = 0;
        }

        if (st.sel >= st.rowCount) {
            st.sel = st.rowCount - 1;
        }

        st.focus.top = false;
    }

    /// @brief Present two black frames (used when launching).
    void Blackout()
    {
        Present(true);
        Present(true);
    }

    /// @brief Activate whatever is focused (top bar, bottom bar, title).
    /// @return True when the A press was consumed.
    bool Activate()
    {
        AppState &st = s_state;

        if (WAlbum::IsOpen()) {
            return false;
        }

        if (st.focus.top) {
            Layout::ActivateTop();
            return true;
        }

        if (st.focus.bottom) {
            ActivateBottom();
            return true;
        }

        int homeIdx = Tabs::FindById("home");

        if (st.menu.cur != homeIdx || st.focus.bottom || st.focus.top ||
            (st.home.sub != HOMESUB_GAMES && st.home.sub != HOMESUB_VGC)) {
            return false;
        }

        if (st.sel < 0 || st.sel >= st.rowCount) {
            return false;
        }

        AppRow *r = &st.rows[st.sel];

        if (r->isGame) {
            if (r->ejected) {
                Sfx::Play(Sfx::Back);
                return true;
            }

            u64 susp = qext_suspended_title();

            if (susp != 0 && susp == r->tid && qext_game_running() &&
                !qext_game_has_foreground()) {
                st.launchBlack = true;
                Blackout();

                if (R_FAILED(qext_resume_game())) {
                    st.launchBlack = false;
                    qext_terminate_game();
                    Sfx::Play(Sfx::Back);
                } else {
                    Sfx::Play(Sfx::Click);
                }

                return true;
            } else if (!qext_game_has_foreground()) {
                st.launchBlack = true;
                Blackout();

                if (R_FAILED(qext_launch_title(r->tid))) {
                    st.launchBlack = false;
                    Sfx::Play(Sfx::Back);
                } else {
                    Sfx::Play(Sfx::Click);
                }

                return true;
            }

            return false;
        }

        qext_launch_applet(r->applet);
        Sfx::Play(Sfx::Click);
        return true;
    }

    /// @brief Main per-frame pump: input, anim, present, audio.
    void Loop()
    {
        AppState &st = s_state;

        if (!st.padReady) {
            return;
        }

        padUpdate(&st.pad);
        u64 down = padGetButtonsDown(&st.pad);
        u64 held = padGetButtons(&st.pad);

        PadIcon::Tick();
        UpdateMenuAnim();
        UpdateHomeShift();

        if (st.focus.topDebounce > 0) {
            st.focus.topDebounce--;
        }

        if (WAlbum::IsOpen() && st.menu.t >= 1.0f) {
            Layout::Input(down, held);
        } else if (st.menu.t < 1.0f) {
            /* Lock input during transitions. */
        } else if (down & HidNpadButton_L) {
            TopSwitch(-1);
        } else if (down & HidNpadButton_R) {
            TopSwitch(1);
        } else {
            int homeIdx = Tabs::FindById("home");

            if (st.menu.cur == homeIdx && !st.focus.top) {
                if (down & HidNpadButton_X) {
                    qext_terminate_game();
                }

                if (down & HidNpadButton_Plus) {
                    RefreshTitles();
                }
            }

            if (down & HidNpadButton_A) {
                if (Activate()) {
                    down &= ~HidNpadButton_A;
                }
            }

            Layout::Input(down, held);
        }

        /* Scrolling moves the icon budget; re-sync without reloading. */
        if (st.sel != s_iconSel || st.rowCount != s_iconRows)
            AssignIconSlots();

        st.frame++;
        st.tick++;

        if ((st.frame % 60) == 0) {
            Clock::Tick();
        }

        if (st.frame >= 150) {
            st.frame = 0;
            RefreshTitles();
        }

        if (qext_titles_changed()) {
            RefreshTitles();
        }

        Present(st.launchBlack);
        Sfx::Tick();
    }

    /// @brief Handle foregrounding: clear launch black, refresh caches.
    void OnOpen()
    {
        AppState &st = s_state;
        st.launchBlack = false;
        Font::Refresh();
        RefreshTitles();
        Avatars::Refresh();
        WAlbum::OnRefresh();
    }

    /// @brief Handle theme/content reload.
    void OnReload()
    {
        s_state.launchBlack = false;
        Font::Refresh();
        RefreshTitles();
        WAlbum::OnRefresh();
    }

    /// @brief Handle HOME press: close album, snap back to home tab.
    void OnHome()
    {
        AppState &st = s_state;

        if (WAlbum::IsOpen()) {
            WAlbum::Close();
        }

        int homeIdx = Tabs::FindById("home");

        if (homeIdx < 0) {
            homeIdx = 0;
        }

        st.menu.cur = homeIdx;
        st.menu.from = homeIdx;
        st.menu.t = 1.0f;
        st.menu.phase = MenuPhase_Idle;
        st.focus.topSel = homeIdx;
        st.sel = 0;
        st.focus.top = false;
        st.home.sub = HOMESUB_GAMES;
        st.home.settingsPage = -1;
        st.home.settingsRow = 0;
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
    /// @param m Tab index.
    void SwitchMenu(int m)
    {
        AppState &st = s_state;

        if (m < 0 || m >= Tabs::Count()) {
            return;
        }

        st.focus.top = false;
        st.focus.bottom = false;

        const MenuTab *tab = Tabs::Get(m);
        bool isAlbum = tab && strcmp(tab->id, "album") == 0;

        if (isAlbum) {
            if (st.menu.cur != m) {
                if (st.menu.phase == MenuPhase_Idle) {
                    st.menu.from = st.menu.cur;
                }

                st.menu.cur = m;
                st.menu.dir = (m > st.menu.from) ? 1 : -1;
                st.menu.t = 0.0f;
                st.menu.phase = MenuPhase_Animating;
            }

            st.focus.topSel = m;

            if (tab->onEnter) {
                tab->onEnter();
            }

            return;
        }

        if (m == st.menu.cur && st.menu.t >= 1.0f) {
            return;
        }

        st.menu.from = st.menu.cur;
        st.menu.cur = m;
        st.menu.dir = (st.menu.cur > st.menu.from) ? 1 : -1;

        if (st.menu.cur == st.menu.from) {
            st.menu.dir = 1;
        }

        st.menu.t = 0.0f;
        st.menu.phase = MenuPhase_Animating;
        st.focus.topSel = m;
        st.focus.top = false;
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
        AppState &st = s_state;

        if (st.menu.phase == MenuPhase_Idle) {
            return;
        }

        st.menu.t += 1.0f / 15.0f;

        if (st.menu.t >= 1.0f) {
            st.menu.t = 1.0f;
            st.menu.phase = MenuPhase_Idle;

            const MenuTab *cur = Tabs::Get(st.menu.cur);
            bool curIsAlbum = cur && strcmp(cur->id, "album") == 0;

            if (!curIsAlbum && WAlbum::IsOpen()) {
                WAlbum::Close();
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

        if (s_state.home.sub == HOMESUB_FOLDERS) {
            target = -830.0f;
        } else if (s_state.home.sub == HOMESUB_VGC) {
            target = 935.0f;
        }

        float d = target - s_state.home.shiftY;
        return d <= -1.0f || d >= 1.0f;
    }

    /// @brief Tick the home shift spring toward its target.
    void UpdateHomeShift()
    {
        float target = 0.0f;

        if (s_state.home.sub == HOMESUB_FOLDERS) {
            target = -830.0f;
        } else if (s_state.home.sub == HOMESUB_VGC) {
            target = 935.0f;
        }

        float d = target - s_state.home.shiftY;

        if (d > -1.0f && d < 1.0f) {
            s_state.home.shiftY = target;
        } else {
            s_state.home.shiftY += d * 0.12f;
        }
    }

    /// @brief Give the settings page (-1 = option list).
    /// @return Page index.
    int SettingsPage()
    {
        return s_state.home.settingsPage;
    }

    /// @brief Set the settings page and reset the row cursor.
    /// @param p New page.
    void SetSettingsPage(int p)
    {
        s_state.home.settingsPage = p;
        s_state.home.settingsRow = 0;
    }

    /// @brief Give the settings row cursor.
    /// @return Row index.
    int SettingsRow()
    {
        return s_state.home.settingsRow;
    }

    /// @brief Set the settings row cursor.
    /// @param r New row.
    void SetSettingsRow(int r)
    {
        s_state.home.settingsRow = r < 0 ? 0 : r;
    }

    /// @brief Whether the shop shows news instead of the grid.
    /// @return True for news.
    bool EShopNews()
    {
        return s_state.home.eshopNews;
    }

    /// @brief Set the shop news flag.
    /// @param n New flag.
    void SetEShopNews(bool n)
    {
        s_state.home.eshopNews = n;
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
        AppState &st = s_state;

        if (HomeShiftBusy()) {
            return;
        }

        /* Bottom-middle icons: folders / game cards toggle their Home
           sub-screen, sleep sleeps the console. */
        if (BottomSel() == 2) {
            qext_sleep();
            return;
        }

        int want = BottomSel() == 0 ? HOMESUB_FOLDERS : HOMESUB_VGC;

        if (BottomSel() == 0) {
            Sfx::PlayByName("folder_jingle");
        } else {
            Sfx::PlayByName("vgc_jingle");
        }

        SetHomeSub(want == HomeSub() ? HOMESUB_GAMES : want);
        SetBottomFocus(false);
    }

    /// @brief Give the friends selection cursor.
    /// @return Index.
    int FriendSel()
    {
        return s_state.home.friendSel;
    }

    /// @brief Set the friends selection cursor.
    /// @param s New index.
    void SetFriendSel(int s)
    {
        s_state.home.friendSel = s < 0 ? 0 : s;
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

} /* namespace App */
