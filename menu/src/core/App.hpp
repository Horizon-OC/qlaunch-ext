/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */
#pragma once
#include <switch.h>

enum { APP_MAX_TITLES = 64, APP_NAME_MAX = 192, APP_APPLETS = 2 };

struct AppRow {
    bool isGame;
    u64 tid;
    int applet; /* 0 album, 1 controllers */
    int iconSlot;
    char name[APP_NAME_MAX];
};

namespace App {
bool Boot(NWindow *win);
void Loop();
void Shutdown();
void OnOpen();
void OnReload();
void OnHome();
void OnPower();

void RefreshTitles();
void Activate();
void Blackout();

int RowCount();
const AppRow &Row(int i);
int Sel();
void SetSel(int s);
bool DockFocus();
void SetDockFocus(bool f);
int DockSel();
void SetDockSel(int s);
void MoveSel(int delta);
bool LaunchBlack();
} /* namespace App */

