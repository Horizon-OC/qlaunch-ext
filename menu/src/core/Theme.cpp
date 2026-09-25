/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Theme.hpp"
#include "Icons.hpp"
#include "Gfx.hpp"
#include "Json.hpp"
#include "Qext.hpp"
#include "Assets.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace Theme {

enum { MAX_COLORS = 64, MAX_METRICS = 32 };
enum { MAX_THEMES = 16, THEME_COLS = 13 };

struct Entry {
    char name[24];
    float a, b, c;
};

static const char *kColKeys[THEME_COLS] = {
    "bd_home", "bd_connect", "bd_eshop", "bd_album", "bd_settings",
    "menubar", "menugray", "font_pressed", "ac_home", "ac_connect",
    "ac_eshop", "ac_album", "ac_settings",
};

struct ThemeRow {
    char name[16];
    bool dark;
    float col[THEME_COLS][3];
    float ink[3];
};

static char g_name[32];
static Entry g_colors[MAX_COLORS];
static int g_nColors;
static Entry g_metrics[MAX_METRICS];
static int g_nMetrics;

static ThemeRow g_themes[MAX_THEMES];
static int g_nThemes;
static int g_theme = -1;

static bool ParseHex(const char *s, float *r, float *g, float *b)
{
    while (*s == 35 || *s == 32)
        s++;
    unsigned v = 0;
    int n = 0;
    while ((s[n] >= 48 && s[n] <= 57) || (s[n] >= 97 && s[n] <= 102) ||
           (s[n] >= 65 && s[n] <= 70)) {
        unsigned d = 0;
        if (s[n] >= 48 && s[n] <= 57)
            d = (unsigned)(s[n] - 48);
        else if (s[n] >= 97 && s[n] <= 102)
            d = (unsigned)(s[n] - 87);
        else
            d = (unsigned)(s[n] - 55);
        v = (v << 4) | d;
        n++;
    }
    if (n == 6) {
        *r = ((v >> 16) & 0xFF) / 255.0f;
        *g = ((v >> 8) & 0xFF) / 255.0f;
        *b = (v & 0xFF) / 255.0f;
        return true;
    }
    if (n == 3) {
        *r = (((v >> 8) & 0xF) * 17) / 255.0f;
        *g = (((v >> 4) & 0xF) * 17) / 255.0f;
        *b = ((v & 0xF) * 17) / 255.0f;
        return true;
    }
    return false;
}

static void StoreColor(const char *key, float r, float g, float b)
{
    for (int i = 0; i < g_nColors; i++) {
        if (strcmp(g_colors[i].name, key) == 0) {
            g_colors[i].a = r;
            g_colors[i].b = g;
            g_colors[i].c = b;
            return;
        }
    }
    if (g_nColors >= MAX_COLORS)
        return;
    Entry *e = &g_colors[g_nColors++];
    size_t kl = strlen(key);
    if (kl > sizeof(e->name) - 1)
        kl = sizeof(e->name) - 1;
    memcpy(e->name, key, kl);
    e->name[kl] = 0;
    e->a = r;
    e->b = g;
    e->c = b;
}

static void StoreMetric(const char *key, float v)
{
    for (int i = 0; i < g_nMetrics; i++) {
        if (strcmp(g_metrics[i].name, key) == 0) {
            g_metrics[i].a = v;
            return;
        }
    }
    if (g_nMetrics >= MAX_METRICS)
        return;
    Entry *e = &g_metrics[g_nMetrics++];
    size_t kl = strlen(key);
    if (kl > sizeof(e->name) - 1)
        kl = sizeof(e->name) - 1;
    memcpy(e->name, key, kl);
    e->name[kl] = 0;
    e->a = v;
    e->b = 0.0f;
    e->c = 0.0f;
}

static void ApplyRow(int idx)
{
    const ThemeRow *t = &g_themes[idx];
    g_nColors = 0;
    for (int i = 0; i < THEME_COLS; i++)
        StoreColor(kColKeys[i], t->col[i][0], t->col[i][1], t->col[i][2]);
    
    StoreColor("bg", t->col[0][0], t->col[0][1], t->col[0][2]);
    StoreColor("panel", t->col[5][0], t->col[5][1], t->col[5][2]);
    StoreColor("dock", t->col[5][0], t->col[5][1], t->col[5][2]);
    StoreColor("ink", t->ink[0], t->ink[1], t->ink[2]);
    StoreColor("dim", t->col[6][0], t->col[6][1], t->col[6][2]);
    StoreColor("accent", 0.0f, 0.7647f, 0.8902f);
    StoreColor("selring", 0.498f, 0.7216f, 0.9333f);
    StoreColor("good", 0.2078f, 0.7804f, 0.3490f);
    StoreColor("warn", 0.9412f, 0.6275f, 0.1255f);
    StoreColor("bad", 0.9412f, 0.3137f, 0.3137f);
    StoreColor("tile_game", 0.557f, 0.557f, 0.588f);
    StoreColor("tile_album", 0.2902f, 0.5647f, 0.8510f);
    StoreColor("tile_ctrl", 0.3608f, 0.7216f, 0.3608f);
    StoreColor("hl0", 0.0f, 0.4f, 1.0f);
    StoreColor("hl1", 0.9333f, 1.0f, 1.0f);
    StoreColor("hl2", 0.5333f, 0.4667f, 1.0f);
    StoreColor("hl3", 1.0f, 0.8667f, 1.0f);
    StoreColor("hl1deep", 0.6667f, 0.9333f, 1.0f);
    StoreColor("hl3deep", 1.0f, 0.7333f, 1.0f);
    StoreColor("avatar", 0.9412f, 0.3843f, 0.5725f);
    if (t->dark)
        StoreColor("battery", 0.8157f, 0.8157f, 0.8471f);
    else
        StoreColor("battery", 0.3333f, 0.3333f, 0.3608f);
    size_t nl = strlen(t->name);
    if (nl > sizeof(g_name) - 1)
        nl = sizeof(g_name) - 1;
    memcpy(g_name, t->name, nl);
    g_name[nl] = 0;
}

static bool LoadTable(const char *json, unsigned len)
{
    JxDoc doc;
    if (!Jx::Parse(doc, json, len))
        return false;
    if (Jx::TokType(doc, 0) != 1)
        return false;
    int ta = Jx::ObjGet(doc, 0, "t");
    if (Jx::TokType(doc, ta) != 2)
        return false;
    int n = Jx::ArrSize(doc, ta);
    g_nThemes = 0;
    for (int i = 0; i < n && g_nThemes < MAX_THEMES; i++) {
        int row = Jx::ArrAt(doc, ta, i);
        if (Jx::TokType(doc, row) != 2 || Jx::ArrSize(doc, row) < 16)
            continue;
        ThemeRow *t = &g_themes[g_nThemes];
        memset(t, 0, sizeof(*t));
        Jx::Str(doc, Jx::ArrAt(doc, row, 0), t->name, sizeof(t->name));
        t->dark = Jx::Num(doc, Jx::ArrAt(doc, row, 1), 0) != 0;
        char hb[16];
        for (int c = 0; c < THEME_COLS; c++) {
            hb[0] = 0;
            Jx::Str(doc, Jx::ArrAt(doc, row, 2 + c), hb, sizeof(hb));
            if (!ParseHex(hb, &t->col[c][0], &t->col[c][1], &t->col[c][2])) {
                t->col[c][0] = 1.0f;
                t->col[c][1] = 0.0f;
                t->col[c][2] = 1.0f;
            }
        }
        hb[0] = 0;
        Jx::Str(doc, Jx::ArrAt(doc, row, 15), hb, sizeof(hb));
        if (!ParseHex(hb, &t->ink[0], &t->ink[1], &t->ink[2])) {
            t->ink[0] = 0.15f;
            t->ink[1] = 0.15f;
            t->ink[2] = 0.15f;
        }
        g_nThemes++;
    }
    return g_nThemes > 0;
}

bool LoadJson(const char *json, unsigned len)
{
    JxDoc doc;
    if (!Jx::Parse(doc, json, len))
        return false;
    if (Jx::TokType(doc, 0) != 1)
        return false;
    g_nColors = 0;
    g_nMetrics = 0;
    g_name[0] = 0;
    Jx::Str(doc, Jx::ObjGet(doc, 0, "name"), g_name, sizeof(g_name));
    int co = Jx::ObjGet(doc, 0, "colors");
    if (Jx::TokType(doc, co) == 1) {
        int span = Jx::TokAfter(doc, co);
        for (int t = co + 1; t < span;) {
            char key[24] = { 0 };
            if (Jx::TokType(doc, t) != 3)
                break;
            Jx::Str(doc, t, key, sizeof(key));
            char val[16] = { 0 };
            Jx::Str(doc, t + 1, val, sizeof(val));
            float r = 1.0f, g = 0.0f, b = 1.0f;
            ParseHex(val, &r, &g, &b);
            StoreColor(key, r, g, b);
            t = Jx::TokAfter(doc, t + 1);
        }
    }
    int mo = Jx::ObjGet(doc, 0, "metrics");
    if (Jx::TokType(doc, mo) == 1) {
        int span = Jx::TokAfter(doc, mo);
        for (int t = mo + 1; t < span;) {
            char key[24] = { 0 };
            if (Jx::TokType(doc, t) != 3)
                break;
            Jx::Str(doc, t, key, sizeof(key));
            StoreMetric(key, (float)Jx::Num(doc, t + 1, 0));
            t = Jx::TokAfter(doc, t + 1);
        }
    }
    return true;
}

void Color(const char *key, float *r, float *g, float *b)
{
    for (int i = 0; i < g_nColors; i++) {
        if (strcmp(g_colors[i].name, key) == 0) {
            *r = g_colors[i].a;
            *g = g_colors[i].b;
            *b = g_colors[i].c;
            return;
        }
    }
    *r = 1.0f;
    *g = 0.0f;
    *b = 1.0f;
}

float Metric(const char *key, float dflt)
{
    for (int i = 0; i < g_nMetrics; i++) {
        if (strcmp(g_metrics[i].name, key) == 0)
            return g_metrics[i].a;
    }
    return dflt;
}

const char *Name()
{
    return g_name;
}

int ThemeCount()
{
    return g_nThemes;
}

const char *ThemeName(int i)
{
    if (i < 0 || i >= g_nThemes)
        return "";
    return g_themes[i].name;
}

int GetTheme()
{
    return g_theme;
}

bool IsDark()
{
    return g_theme >= 0 && g_theme < g_nThemes && g_themes[g_theme].dark;
}

void MenuColor(int menu, float *r, float *g, float *b)
{
    if (g_theme < 0 || g_theme >= g_nThemes || menu < 0 || menu > 4) {
        *r = 1.0f;
        *g = 1.0f;
        *b = 1.0f;
        return;
    }
    *r = g_themes[g_theme].col[menu][0];
    *g = g_themes[g_theme].col[menu][1];
    *b = g_themes[g_theme].col[menu][2];
}

void MenuAccent(int menu, float *r, float *g, float *b)
{
    if (g_theme < 0 || g_theme >= g_nThemes || menu < 0 || menu > 4) {
        *r = 0.5f;
        *g = 0.5f;
        *b = 0.5f;
        return;
    }
    *r = g_themes[g_theme].col[8 + menu][0];
    *g = g_themes[g_theme].col[8 + menu][1];
    *b = g_themes[g_theme].col[8 + menu][2];
}

void ThemePreview(int t, float *r, float *g, float *b)
{
    if (t < 0 || t >= g_nThemes) {
        *r = 0.5f;
        *g = 0.5f;
        *b = 0.5f;
        return;
    }
    *r = g_themes[t].col[0][0];
    *g = g_themes[t].col[0][1];
    *b = g_themes[t].col[0][2];
}

static void PersistTheme()
{
    char buf[48];
    snprintf(buf, sizeof(buf), "{\"theme\": %d}", g_theme);
    FILE *f = fopen("sdmc:/qlaunch-ext/theme.json", "wb");
    if (!f)
        return;
    fwrite(buf, 1, strlen(buf), f);
    fclose(f);
}

bool SetTheme(int i)
{
    if (i < 0 || i >= g_nThemes)
        return false;
    if (!g_nMetrics) {
        StoreMetric("corner", 30.0f);
        StoreMetric("frame", 10.0f);
        StoreMetric("dock_h", 110.0f);
        StoreMetric("hl_thick", 10.0f);
        StoreMetric("hl_rot", 6.0f);
        StoreMetric("hl_pulse", 1.0f);
        StoreMetric("menu_slide", 0.5f);
        StoreMetric("sub_slide", 1.0f);
        StoreMetric("top_debounce", 0.65f);
    }
    g_theme = i;
    ApplyRow(i);
    if (Gfx::Ready()) {
        Icons::ResetHud();
        Icons::InitHud();
    }
    PersistTheme();
    return true;
}

bool LoadBuiltin(const char *name)
{
    if (!g_nThemes &&
        !LoadTable(themes_json, themes_json_size))
        return LoadJson(
            (name && strcmp(name, "dark") == 0) ? theme_dark_json : theme_light_json,
            (name && strcmp(name, "dark") == 0) ? theme_dark_json_size : theme_light_json_size);
    if (name && strcmp(name, "dark") == 0)
        return SetTheme(1);
    return SetTheme(0);
}

bool LoadFile(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 65536) {
        fclose(f);
        return false;
    }
    char *buf = (char *)malloc((unsigned)sz + 1);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t got = fread(buf, 1, (unsigned)sz, f);
    fclose(f);
    if (got != (unsigned)sz) {
        free(buf);
        return false;
    }
    buf[got] = 0;

    JxDoc doc;
    bool ok = false;
    if (Jx::Parse(doc, buf, (unsigned)got) && Jx::TokType(doc, 0) == 1) {
        int tt = Jx::ObjGet(doc, 0, "theme");
        if (Jx::TokType(doc, tt) == 4) {
            if (!g_nThemes)
                LoadTable(themes_json, themes_json_size);
            ok = SetTheme((int)Jx::Num(doc, tt, 0));
            free(buf);
            return ok;
        }
    }
    ok = LoadJson(buf, (unsigned)got);
    free(buf);
    return ok;
}

bool InitAuto()
{
    if (!g_nThemes)
        LoadTable(themes_json, themes_json_size);
    if (LoadFile("sdmc:/qlaunch-ext/theme.json"))
        return true;
    ColorSetId cs = ColorSetId_Light;
    if (R_SUCCEEDED(setsysGetColorSetId(&cs)) && cs == ColorSetId_Dark)
        return SetTheme(g_nThemes > 1 ? 1 : 0);
    return SetTheme(0);
}

} /* namespace Theme */