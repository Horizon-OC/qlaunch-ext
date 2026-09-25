/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */
#pragma once
#include <switch.h>
#include "Theme.hpp"

enum { APP_MAX_TITLES = 64, APP_NAME_MAX = 192, APP_APPLETS = 2 };

enum { HOMESUB_GAMES = 0, HOMESUB_FOLDERS, HOMESUB_VGC };

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

bool TopFocus();
void SetTopFocus(bool f);
int TopSel();
void SetTopSel(int s);

int Menu();
int MenuFrom();
float MenuT(); /* Progress from 0-1*/
int MenuDir();
void SwitchMenu(int m);
void UpdateMenuAnim();

int HomeSub();
void SetHomeSub(int s);
bool BottomFocus();
void SetBottomFocus(bool f);
int BottomSel();
void SetBottomSel(int s);
void ActivateBottom();
float HomeShiftY();
bool HomeShiftBusy();
void UpdateHomeShift();

int SettingsPage();
void SetSettingsPage(int p);
int SettingsRow();
void SetSettingsRow(int r);

bool EShopNews();
void SetEShopNews(bool n);

int FriendSel();
void SetFriendSel(int s);
float Time();
void MoveSel(int delta);
bool LaunchBlack();
} /* namespace App */