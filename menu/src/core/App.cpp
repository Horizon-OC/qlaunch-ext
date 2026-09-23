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
#include "Qext.hpp"
#include "Log.hpp"
#include <string.h>

namespace App {

static AppRow rows_[APP_MAX_TITLES + APP_APPLETS];
static int rowCount_;
static int sel_;
static bool dockFocus_;
static int dockSel_;
static bool launchBlack_;
static PadState pad_;
static bool padReady_;
static unsigned frame_;
static unsigned holdFrames_;

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
    if (!Layout::LoadAuto()) {
        Shutdown();
        return false;
    }

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad_);
    padReady_ = true;

    Clock::Tick();
    sel_ = 0;
    dockFocus_ = false;
    dockSel_ = 0;
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
    static const char *const appletNames[APP_APPLETS] = { "Album", "Controllers" };
    u64 keepTid = 0;
    int keepApplet = -2;
    bool keepGame = true;
    if (sel_ >= 0 && sel_ < rowCount_) {
        keepGame = rows_[sel_].isGame;
        keepTid = rows_[sel_].tid;
        keepApplet = rows_[sel_].applet;
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
    for (int k = 0; k < APP_APPLETS; k++) {
        AppRow *a = &rows_[rowCount_++];
        a->isGame = false;
        a->tid = 0;
        a->applet = k;
        a->iconSlot = -1;
        size_t len = strlen(appletNames[k]) + 1;
        if (len > APP_NAME_MAX)
            len = APP_NAME_MAX;
        memcpy(a->name, appletNames[k], len);
        a->name[APP_NAME_MAX - 1] = 0;
    }
    int want = -1;
    for (int i = 0; i < rowCount_; i++) {
        if (rows_[i].isGame == keepGame && rows_[i].tid == keepTid &&
            rows_[i].applet == keepApplet) {
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
    if (dockSel_ < 0)
        dockSel_ = 0;
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
    dockFocus_ = false;
}

void Present(bool black)
{
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
    int slots[GFX_ICONQ_MAX / 6];
    Layout::IconSlots(slots);
    Gfx::DrawIcons(slots);
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

void Activate()
{
    if (sel_ < 0 || sel_ >= rowCount_)
        return;
    if (dockFocus_) {
        Layout::ActivateDock();
        return;
    }
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

    if (down & HidNpadButton_X)
        qext_terminate_game();
    if (down & HidNpadButton_Plus)
        RefreshTitles();
    if (down & HidNpadButton_A)
        Activate();
    Layout::Input(down, held);

    frame_++;
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
}

void OnReload()
{
    launchBlack_ = false;
    Font::Refresh();
    RefreshTitles();
}

void OnHome()
{
    sel_ = 0;
    dockFocus_ = false;
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

bool DockFocus()
{
    return dockFocus_;
}

void SetDockFocus(bool f)
{
    dockFocus_ = f;
}

int DockSel()
{
    return dockSel_;
}

void SetDockSel(int s)
{
    dockSel_ = s;
}

bool LaunchBlack()
{
    return launchBlack_;
}

} /* namespace App */

