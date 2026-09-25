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
#include "PadIcon.hpp"
#include "../widgets/Widgets.hpp"
#include "Avatars.hpp"
#include "Qext.hpp"
#include "Log.hpp"
#include <string.h>

namespace App {

static AppRow rows_[APP_MAX_TITLES + APP_APPLETS];
static int rowCount_;
static int sel_;
static bool topFocus_;
static int topSel_;
static int menu_;
static int menuFrom_;
static float menuT_;
static int menuDir_;
static unsigned topDebounce_;
static int homeSub_;
static float homeShiftY_;
static bool bottomFocus_;
static int bottomSel_;
static int settingsPage_;
static int settingsRow_;
static bool eshopNews_;
static int friendSel_;
static bool launchBlack_;
static PadState pad_;
static bool padReady_;
static unsigned frame_;
static unsigned holdFrames_;
static unsigned tick_;

static void Reload();
static void Present(bool black);

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
    if (!Gfx::Init(win, fbW, fbH))
        return false;
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
    if (!Layout::LoadAuto()) {
        Shutdown();
        return false;
    }

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad_);
    padReady_ = true;

    Clock::Tick();
    Avatars::Refresh();
    sel_ = 0;
    topFocus_ = false;
    topSel_ = 0;
    menu_ = MENU_HOME;
    menuFrom_ = MENU_HOME;
    menuT_ = 1.0f;
    menuDir_ = 1;
    topDebounce_ = 0;
    homeSub_ = HOMESUB_GAMES;
    homeShiftY_ = 0.0f;
    bottomFocus_ = false;
    bottomSel_ = 0;
    settingsPage_ = -1;
    settingsRow_ = 0;
    eshopNews_ = false;
    friendSel_ = 0;
    launchBlack_ = false;
    frame_ = 0;
    holdFrames_ = 0;
    RefreshTitles();
    return true;
}

void Shutdown()
{
    padReady_ = false;
    Icons::FreeAll();
    Font::Shutdown();
    Gfx::Destroy();
}

void RefreshTitles()
{
    qext_refresh_titles();
    Reload();
    u64 tids[APP_MAX_TITLES + APP_APPLETS];
    int slots[APP_MAX_TITLES + APP_APPLETS];
    for (int i = 0; i < rowCount_; i++) {
        rows_[i].iconSlot = -1;
        tids[i] = rows_[i].isGame ? rows_[i].tid : 0;
        slots[i] = -1;
    }
    Icons::Sync(tids, slots, rowCount_);
    for (int i = 0; i < rowCount_; i++)
        rows_[i].iconSlot = slots[i];
}

void Reload()
{
    static const char fallback[] = "Unknown title";
    u64 keepTid = 0;
    if (sel_ >= 0 && sel_ < rowCount_) {
        keepTid = rows_[sel_].tid;
    }
    int n = qext_title_count();
    if (n < 0)
        n = 0;
    if (n > APP_MAX_TITLES)
        n = APP_MAX_TITLES;
    rowCount_ = 0;
    for (int i = 0; i < n; i++) {
        AppRow *r = &rows_[rowCount_++];
        r->isGame = true;
        r->tid = qext_title_id(i);
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
    for (int i = 0; i < rowCount_; i++) {
        if (rows_[i].tid == keepTid) {
            want = i;
            break;
        }
    }
    if (want >= 0)
        sel_ = want;
    else if (sel_ >= rowCount_)
        sel_ = rowCount_ > 0 ? rowCount_ - 1 : 0;
    if (sel_ < 0)
        sel_ = 0;
    if (topSel_ < 0)
        topSel_ = 0;
    if (topSel_ > 4)
        topSel_ = 4;
}

void MoveSel(int delta)
{
    if (rowCount_ <= 0)
        return;
    sel_ += delta;
    if (sel_ < 0)
        sel_ = 0;
    if (sel_ >= rowCount_)
        sel_ = rowCount_ - 1;
    topFocus_ = false;
}

void Present(bool black)
{
    Gfx::WaitIdle();
    Gfx::ResetCounts();
    if (black) {
        Gfx::PushQuad(0.0f, 0.0f, 1920.0f, 1080.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    } else {
        Layout::Draw();
    }
    if (!Gfx::BeginFrame())
        return;
    Gfx::DrawColor();
    Gfx::DrawPanelsBg();
    Gfx::DrawPanels();
    Gfx::DrawHighlights();
    Gfx::DrawIcons();
    Gfx::DrawTextBuf(0, 0);
    Gfx::DrawTextBuf(1, 1);
    Gfx::DrawTextBuf(2, 2);
    Gfx::EndFrame();
}

void Blackout()
{
    Present(true);
    Present(true);
}

static void TopSwitch(int delta)
{
    if (topDebounce_ > 0)
        return;
    topDebounce_ = 39;
    int m = (menu_ + delta + MENU_COUNT) % MENU_COUNT;
    topSel_ = m;
    SwitchMenu(m);
}

void Activate()
{
    if (WAlbum::IsOpen())
        return;
    if (topFocus_) {
        Layout::ActivateTop();
        return;
    }
    if (bottomFocus_) {
        ActivateBottom();
        return;
    }
    if (menu_ != MENU_HOME || bottomFocus_ || topFocus_ ||
        (homeSub_ != HOMESUB_GAMES && homeSub_ != HOMESUB_VGC))
        return;
    if (sel_ < 0 || sel_ >= rowCount_)
        return;
    AppRow *r = &rows_[sel_];
    if (r->isGame) {
        u64 susp = qext_suspended_title();
        if (susp != 0 && susp == r->tid && qext_game_running() &&
            !qext_game_has_foreground()) {
            launchBlack_ = true;
            Blackout();
            qext_resume_game();
        } else if (!qext_game_has_foreground()) {
            launchBlack_ = true;
            Blackout();
            qext_launch_title(r->tid);
        }
    } else {
        qext_launch_applet(r->applet);
    }
}

void Loop()
{
    if (!padReady_)
        return;
    padUpdate(&pad_);
    u64 down = padGetButtonsDown(&pad_);
    u64 held = padGetButtons(&pad_);

    PadIcon::Tick();
    UpdateMenuAnim();
    UpdateHomeShift();
    if (topDebounce_ > 0)
        topDebounce_--;
    if (WAlbum::IsOpen()) {
        Layout::Input(down, held);
    } else if (menuT_ < 1.0f) {
        /* menus.gd switching: input locked mid-slide. */
    } else if (down & HidNpadButton_L) {
        TopSwitch(-1);
    } else if (down & HidNpadButton_R) {
        TopSwitch(1);
    } else {
        if (menu_ == MENU_HOME && !topFocus_) {
            if (down & HidNpadButton_X)
                qext_terminate_game();
            if (down & HidNpadButton_Plus)
                RefreshTitles();
        }
        if (down & HidNpadButton_A)
            Activate();
        Layout::Input(down, held);
    }

    frame_++;
    tick_++;
    if ((frame_ % 60) == 0)
        Clock::Tick();
    if (frame_ >= 300) {
        frame_ = 0;
        RefreshTitles();
    }
    Present(launchBlack_);
}

void OnOpen()
{
    launchBlack_ = false;
    Font::Refresh();
    RefreshTitles();
    Avatars::Refresh();
    WAlbum::OnRefresh();
}

void OnReload()
{
    launchBlack_ = false;
    Font::Refresh();
    RefreshTitles();
    WAlbum::OnRefresh();
}

void OnHome()
{
    if (WAlbum::IsOpen())
        WAlbum::Close();
    menu_ = MENU_HOME;
    menuFrom_ = MENU_HOME;
    menuT_ = 1.0f;
    topSel_ = MENU_HOME;
    sel_ = 0;
    topFocus_ = false;
    homeSub_ = HOMESUB_GAMES;
    settingsPage_ = -1;
    settingsRow_ = 0;
}

void OnPower()
{
}

int RowCount()
{
    return rowCount_;
}

const AppRow &Row(int i)
{
    return rows_[i < 0 ? 0 : i];
}

int Sel()
{
    return sel_;
}

void SetSel(int s)
{
    sel_ = s;
}

bool TopFocus()
{
    return topFocus_;
}

void SetTopFocus(bool f)
{
    topFocus_ = f;
}

int TopSel()
{
    return topSel_;
}

void SetTopSel(int s)
{
    if (s < 0)
        s = 0;
    if (s > 4)
        s = 4;
    topSel_ = s;
}

int Menu()
{
    return menu_;
}

int MenuFrom()
{
    return menuFrom_;
}

float MenuT()
{
    float t = menuT_;
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;
    /* Tween.TRANS_CUBIC easeInOut like menus.gd. */
    if (t < 0.5f)
        return 4.0f * t * t * t;
    float u = -2.0f * t + 2.0f;
    return 1.0f - u * u * u / 2.0f;
}

int MenuDir()
{
    return menuDir_;
}

void SwitchMenu(int m)
{
    if (m < 0 || m >= MENU_COUNT)
        return;
    if (m == MENU_ALBUM) {
        if (menu_ != MENU_ALBUM) {
            menuFrom_ = menu_;
            menu_ = MENU_ALBUM;
            menuDir_ = (MENU_ALBUM > menuFrom_) ? 1 : -1;
            menuT_ = 0.0f;
        }
        topSel_ = MENU_ALBUM;
        WAlbum::Open();
        return;
    }
    if (menu_ == MENU_ALBUM && WAlbum::IsOpen())
        WAlbum::Close();
    if (m == menu_ && menuT_ >= 1.0f)
        return;
    menuFrom_ = menu_;
    menu_ = m;
    menuDir_ = (menu_ > menuFrom_) ? 1 : -1;
    if (menu_ == menuFrom_)
        menuDir_ = 1;
    menuT_ = 0.0f;
    topSel_ = m;
    topFocus_ = false;
}

void UpdateMenuAnim()
{
    if (menuT_ < 1.0f) {
        menuT_ += 1.0f / 15.0f;
        if (menuT_ > 1.0f)
            menuT_ = 1.0f;
    }
}

int HomeSub()
{
    return homeSub_;
}

void SetHomeSub(int s)
{
    if (s < 0)
        s = 0;
    if (s > 2)
        s = 2;
    homeSub_ = s;
}

float HomeShiftY()
{
    return homeShiftY_;
}

bool HomeShiftBusy()
{
    float target = 0.0f;
    if (homeSub_ == HOMESUB_FOLDERS)
        target = -830.0f;
    else if (homeSub_ == HOMESUB_VGC)
        target = 935.0f;
    float d = target - homeShiftY_;
    return d <= -1.0f || d >= 1.0f;
}

void UpdateHomeShift()
{
    float target = 0.0f;
    if (homeSub_ == HOMESUB_FOLDERS)
        target = -830.0f;
    else if (homeSub_ == HOMESUB_VGC)
        target = 935.0f;
    float d = target - homeShiftY_;
    if (d > -1.0f && d < 1.0f)
        homeShiftY_ = target;
    else
        homeShiftY_ += d * 0.12f;
}

int SettingsPage()
{
    return settingsPage_;
}

void SetSettingsPage(int p)
{
    settingsPage_ = p;
    settingsRow_ = 0;
}

int SettingsRow()
{
    return settingsRow_;
}

void SetSettingsRow(int r)
{
    settingsRow_ = r < 0 ? 0 : r;
}

bool EShopNews()
{
    return eshopNews_;
}

void SetEShopNews(bool n)
{
    eshopNews_ = n;
}

bool BottomFocus()
{
    return bottomFocus_;
}

void SetBottomFocus(bool f)
{
    bottomFocus_ = f;
}

int BottomSel()
{
    return bottomSel_;
}

void SetBottomSel(int s)
{
    if (s < 0)
        s = 0;
    if (s > 2)
        s = 2;
    bottomSel_ = s;
}

void ActivateBottom()
{
    if (HomeShiftBusy())
        return;
    /* Bottom-middle icons: folders / game cards toggle their Home
       sub-screen, sleep sleeps the console. */
    if (BottomSel() == 2) {
        qext_sleep();
        return;
    }
    int want = BottomSel() == 0 ? HOMESUB_FOLDERS : HOMESUB_VGC;
    SetHomeSub(want == HomeSub() ? HOMESUB_GAMES : want);
    SetBottomFocus(false);
}

int FriendSel()
{
    return friendSel_;
}

void SetFriendSel(int s)
{
    friendSel_ = s < 0 ? 0 : s;
}

bool LaunchBlack()
{
    return launchBlack_;
}

float Time()
{
    return (float)tick_ / 60.0f;
}

} /* namespace App */

