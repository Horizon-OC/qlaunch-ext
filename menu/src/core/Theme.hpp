/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

enum { MENU_HOME = 0, MENU_CONNECT, MENU_ESHOP, MENU_ALBUM, MENU_SETTINGS, MENU_COUNT };

namespace Theme {
bool LoadJson(const char *json, unsigned len);
bool LoadBuiltin(const char *name); /* "light" / "dark" */
bool LoadFile(const char *path);    /* SD override, 64KB max */
bool InitAuto();                    /* SD theme index, else system set */
void Color(const char *key, float *r, float *g, float *b);
float Metric(const char *key, float dflt);
const char *Name();

int ThemeCount();
const char *ThemeName(int i);
bool SetTheme(int i);
int GetTheme();
bool IsDark();

void MenuColor(int menu, float *r, float *g, float *b);
void MenuAccent(int menu, float *r, float *g, float *b);
void ThemePreview(int t, float *r, float *g, float *b);
} /* namespace Theme */