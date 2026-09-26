/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Widgets.hpp"
#include "../core/Gfx.hpp"
#include "../core/Font.hpp"
#include "../core/Theme.hpp"
#include "../core/Layout.hpp"
#include "../core/App.hpp"
#include "../core/Sfx.hpp"
#include "../core/Qext.hpp"
#include <string.h>

namespace WSettings {

/* TODO: Cleanup */
static const char *kOptions[18] = {
    "Controller", "Health", "Airplane", "Bright", "Internet", "BT",
    "TV", "Data", "Lock", "Parent", "Accessibility", "Themes",
    "Users", "Mii", "amiibo", "Noti", "Sleep", "System",
};

enum { ROW_LABEL = 0, ROW_TOGGLE, ROW_ACTION };
enum { ACT_CONTROLLERS = 1, ACT_MII = 2 };

struct SRow {
    const char *label;
    int kind;
    int data;
};

#define ROWS(...) { __VA_ARGS__, { 0, 0, 0 } }

static const SRow kController[] =
    ROWS({ "Change Grip/Order", ROW_ACTION, ACT_CONTROLLERS },
         { "Pro Controller Wired Communication", ROW_TOGGLE, 0 },
         { "NFC Function", ROW_TOGGLE, 1 },
         { "Vibration", ROW_TOGGLE, 2 },
         { "Calibrate Control Sticks", ROW_LABEL, 0 });

static const SRow kHealth[] =
    ROWS({ "Safety Guide", ROW_LABEL, 0 }, { "Precautions", ROW_LABEL, 0 });

static const SRow kAirplane[] =
    ROWS({ "Airplane Mode", ROW_TOGGLE, 0 },
         { "Bluetooth in Airplane Mode", ROW_TOGGLE, 1 });

static const SRow kBright[] =
    ROWS({ "Screen Brightness", ROW_LABEL, 0 },
         { "Auto-Brightness", ROW_TOGGLE, 0 });

static const SRow kInternet[] =
    ROWS({ "Internet Settings", ROW_LABEL, 0 },
         { "Connection Status", ROW_LABEL, 0 });

static const SRow kBT[] =
    ROWS({ "Bluetooth Audio", ROW_TOGGLE, 0 },
         { "Pair New Device", ROW_LABEL, 0 });

static const SRow kTV[] =
    ROWS({ "TV Resolution", ROW_LABEL, 0 },
         { "Match TV Power State", ROW_TOGGLE, 0 });
         
static const SRow kData[] =
    ROWS({ "System Memory", ROW_LABEL, 0 }, { "microSD Card", ROW_LABEL, 0 },
         { "Manage Software", ROW_LABEL, 0 },
         { "Manage Screenshots and Videos", ROW_LABEL, 0 },
         { "Cloud Backup", ROW_LABEL, 0 },
         { "Transfer to System Memory", ROW_LABEL, 0 },
         { "Format microSD Card", ROW_LABEL, 0 });

static const SRow kLock[] =
    ROWS({ "Console Screen Lock", ROW_TOGGLE, 0 },
         { "Change PIN", ROW_LABEL, 0 });

static const SRow kParent[] =
    ROWS({ "Parental Controls Settings", ROW_LABEL, 0 },
         { "Restriction Level", ROW_LABEL, 0 });

static const SRow kAccess[] =
    ROWS({ "Zoom", ROW_TOGGLE, 0 }, { "Grayscale", ROW_TOGGLE, 1 },
         { "Invert Colors", ROW_TOGGLE, 2 },
         { "Button Mapping", ROW_LABEL, 0 });

static const SRow kUsers[] =
    ROWS({ "Add User", ROW_LABEL, 0 }, { "User Settings", ROW_LABEL, 0 });

static const SRow kMii[] =
    ROWS({ "Create Mii", ROW_ACTION, ACT_MII },
         { "Edit Mii", ROW_ACTION, ACT_MII },
         { "Delete Mii", ROW_LABEL, 0 });

static const SRow kAmiibo[] =
    ROWS({ "Register Owner and Nickname", ROW_LABEL, 0 },
         { "Delete Game Data", ROW_LABEL, 0 },
         { "Reset amiibo", ROW_LABEL, 0 });

static const SRow kNoti[] =
    ROWS({ "Notification Display", ROW_TOGGLE, 0 },
         { "Download Notifications", ROW_TOGGLE, 1 });

static const SRow kSleep[] =
    ROWS({ "Auto-Sleep", ROW_TOGGLE, 0 },
         { "Maintain Internet Connection in Sleep", ROW_TOGGLE, 1 },
         { "Wake When AC Adapter Is Unplugged", ROW_TOGGLE, 2 });

static const SRow kSystem[] =
    ROWS({ "System Update", ROW_LABEL, 0 },
         { "Date and Time", ROW_LABEL, 0 }, { "Language", ROW_LABEL, 0 },
         { "Console Nickname", ROW_LABEL, 0 });

static const SRow *PageRows(int page)
{
    switch (page) {
    case 0: return kController;
    case 1: return kHealth;
    case 2: return kAirplane;
    case 3: return kBright;
    case 4: return kInternet;
    case 5: return kBT;
    case 6: return kTV;
    case 7: return kData;
    case 8: return kLock;
    case 9: return kParent;
    case 10: return kAccess;
    case 12: return kUsers;
    case 13: return kMii;
    case 14: return kAmiibo;
    case 15: return kNoti;
    case 16: return kSleep;
    case 17: return kSystem;
    default: return 0;
    }
}

static int RowCount(const SRow *rows)
{
    int n = 0;
    while (rows && rows[n].label)
        n++;
    return n;
}

/* Stubbed for now */
static bool s_tog[18][8];

static void DrawOptions(float shx)
{
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);

    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);

    int page = App::SettingsPage();
    int row = App::SettingsRow();
    float y0 = 258.0f, rh = 34.0f;

    for (int i = 0; i < 18; i++) {
        float y = y0 + (float)i * rh;
        bool sel = (page < 0 && row == i);

        if (sel) {
            float ar, ag, ab;
            Theme::Color("accent", &ar, &ag, &ab);
            Gfx::PushPanel(110.0f + shx, y - 4.0f, 500.0f, rh, 8.0f,
                           ar, ag, ab, 1.0f);
        }

        Font::Draw(kOptions[i], 130.0f + shx, y + 22.0f, 27.0f,
                   sel ? 1.0f : (page == i ? inkR : dimR),
                   sel ? 1.0f : (page == i ? inkG : dimG),
                   sel ? 1.0f : (page == i ? inkB : dimB));
    }
}

static void DrawToggleValue(int page, int i, float x, float y)
{
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);
    bool on = (page >= 0 && page < 18 && i >= 0 && i < 8) ? s_tog[page][i]
                                                          : false;
    const char *s = on ? "ON" : "OFF";
    float w = Font::Measure(s, 30.0f);
    Font::Draw(s, x - w, y, 30.0f, on ? inkR : dimR, on ? inkG : dimG,
               on ? inkB : dimB);
}

static void DrawDetail(float shx)
{
    float inkR, inkG, inkB;
    Theme::Color("ink", &inkR, &inkG, &inkB);
    float dimR, dimG, dimB;
    Theme::Color("dim", &dimR, &dimG, &dimB);
    int page = App::SettingsPage();
    int row = App::SettingsRow();
    if (page < 0 || page >= 18)
        return;
    Font::Draw(kOptions[page], 700.0f + shx, 250.0f, 40.0f, inkR, inkG, inkB);

    if (page == 11) {
        /* Themes page */
        int n = Theme::ThemeCount();
        int cur = Theme::GetTheme();
        float cw = 520.0f, chh = 64.0f, gap = 24.0f;
        for (int i = 0; i < n; i++) {
            int cx = i % 2, cy = i / 2;
            float x = 700.0f + shx + cx * (cw + gap);
            float y = 300.0f + cy * (chh + 12.0f);
            bool sel = (row == i);
            float pr, pg, pb;
            Theme::ThemePreview(i, &pr, &pg, &pb);
            Gfx::PushPanel(x, y, cw, chh, 12.0f, pr, pg, pb, 1.0f);
            if (i == cur || sel) {
                float ar, ag, ab;
                Theme::Color("accent", &ar, &ag, &ab);
                Gfx::PushPanel(x - 4.0f, y - 4.0f, cw + 8.0f, chh + 8.0f,
                               14.0f, ar, ag, ab, 0.35f);
            }
            const char *nm = Theme::ThemeName(i);
            float nw = Font::Measure(nm, 30.0f);
            Font::Draw(nm, x + (cw - nw) * 0.5f, y + 42.0f, 30.0f,
                       inkR, inkG, inkB);
        }
        return;
    }

    const SRow *rows = PageRows(page);
    int n = RowCount(rows);
    float y0 = 320.0f, rh = 64.0f;
    for (int i = 0; i < n; i++) {
        float y = y0 + (float)i * rh;
        bool sel = (row == i);
        if (sel) {
            float ar, ag, ab;
            Theme::Color("accent", &ar, &ag, &ab);
            Gfx::PushPanel(690.0f + shx, y - 6.0f, 1100.0f, rh, 12.0f,
                           ar, ag, ab, 0.30f);
        }
        float lr = sel ? inkR : dimR, lg = sel ? inkG : dimG,
              lb = sel ? inkB : dimB;
        if (rows[i].kind == ROW_TOGGLE) {
            Font::Draw(rows[i].label, 720.0f + shx, y + 40.0f, 32.0f,
                       lr, lg, lb);
            DrawToggleValue(page, i, 1760.0f + shx, y + 40.0f);
        } else {
            Font::Draw(rows[i].label, 720.0f + shx, y + 40.0f, 32.0f,
                       lr, lg, lb);
        }
    }
}

void Draw()
{
    float shx = Layout::ShiftX();
    DrawOptions(shx);
    DrawDetail(shx);
}

static void ActivateRow()
{
    int page = App::SettingsPage();
    int row = App::SettingsRow();
    if (page == 11) {
        Theme::SetTheme(row);
        return;
    }
    const SRow *rows = PageRows(page);
    if (!rows || !rows[row].label)
        return;
    if (rows[row].kind == ROW_TOGGLE) {
        if (row >= 0 && row < 8)
            s_tog[page][row] = !s_tog[page][row];
    } else if (rows[row].kind == ROW_ACTION) {
        if (rows[row].data == ACT_CONTROLLERS)
            qext_launch_applet(1);
        else if (rows[row].data == ACT_MII)
            qext_launch_applet(2);
    }
}

void Input(u64 down, u64 held)
{
    (void)held;
    int page = App::SettingsPage();
    int row = App::SettingsRow();
    if (page < 0) {
        if (down & HidNpadButton_AnyUp) {
            row = (row + 17) % 18;
            if (row != App::SettingsRow()) Sfx::Play(Sfx::Hover);
        App::SetSettingsRow(row);
        }
        if (down & HidNpadButton_AnyDown) {
            row = (row + 1) % 18;
            if (row != App::SettingsRow()) Sfx::Play(Sfx::Hover);
        App::SetSettingsRow(row);
        }
        if (down & HidNpadButton_A) {
            App::SetSettingsPage(row);
            Sfx::Play(Sfx::Click);
        }
        if (down & HidNpadButton_B) {
            App::SwitchMenu(MENU_HOME);
            Sfx::Play(Sfx::Back);
        }
        return;
    }
    int n = (page == 11) ? Theme::ThemeCount()
                         : RowCount(PageRows(page));
    if (n <= 0)
        n = 1;
    int cols = (page == 11) ? 2 : 1;
    if (down & HidNpadButton_AnyLeft) {
        row = (row + n - 1) % n;
        if (row != App::SettingsRow()) Sfx::Play(Sfx::Hover);
        App::SetSettingsRow(row);
    }
    if (down & HidNpadButton_AnyRight) {
        row = (row + 1) % n;
        if (row != App::SettingsRow()) Sfx::Play(Sfx::Hover);
        App::SetSettingsRow(row);
    }
    if (down & HidNpadButton_AnyUp) {
        row = (row + n - cols) % n;
        if (row != App::SettingsRow()) Sfx::Play(Sfx::Hover);
        App::SetSettingsRow(row);
    }
    if (down & HidNpadButton_AnyDown) {
        row = (row + cols) % n;
        if (row != App::SettingsRow()) Sfx::Play(Sfx::Hover);
        App::SetSettingsRow(row);
    }
    if (down & HidNpadButton_A) {
        ActivateRow();
        Sfx::Play(Sfx::Click);
    }
    if (down & HidNpadButton_B)
        App::SetSettingsPage(-1);
        Sfx::Play(Sfx::Back);
}

} /* namespace WSettings */