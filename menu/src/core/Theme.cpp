/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Theme.hpp"
#include "Icons.hpp"
#include "Gfx.hpp"
#include "Json.hpp"
#include "Qext.hpp"
#include "Assets.hpp"
#include "Packs.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace Theme {

constexpr int kThemeMaxColors = 96;
constexpr int kThemeMaxMetrics = 32;
constexpr int kThemeMaxThemes = 16;
constexpr int kThemeColumnCount = 13;
constexpr int kThemeCustomColors = 16;

struct Entry {
    char name[24];
    float a, b, c;
};

static const char *kThemeColumnKeys[kThemeColumnCount] = {
    "bd_home", "bd_connect", "bd_eshop", "bd_album", "bd_settings",
    "menubar", "menugray", "font_pressed", "ac_home", "ac_connect",
    "ac_eshop", "ac_album", "ac_settings",
};

struct ThemeRow {
    char name[16];
    bool dark;
    float col[kThemeColumnCount][3];
    float ink[3];
    char customKey[kThemeCustomColors][24];
    float customValue[kThemeCustomColors][3];
    int customCount;
};

    /// @brief All mutable theme state, single global.
    struct ThemeState {
        char activeName[32];
        Entry colors[kThemeMaxColors];
        int colorCount;
        Entry metrics[kThemeMaxMetrics];
        int metricCount;
        ThemeRow themes[kThemeMaxThemes];
        int themeCount;
        int activeTheme;
        char *themeFileData;
        unsigned themeFileSize;
    };

    static ThemeState s_state = {};

    /// @brief Parse a #RRGGBB or #RGB hex color.
    /// @param s Hex text (leading '#' or spaces skipped).
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    /// @return True on success.
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

    /// @brief Store (or replace) a named color.
    /// @param key Color key.
    /// @param r Red 0-1.
    /// @param g Green 0-1.
    /// @param b Blue 0-1.
    static void StoreColor(const char *key, float r, float g, float b)
{
    for (int i = 0; i < s_state.colorCount; i++) {
        if (strcmp(s_state.colors[i].name, key) == 0) {
            s_state.colors[i].a = r;
            s_state.colors[i].b = g;
            s_state.colors[i].c = b;
            return;
        }
    }
    if (s_state.colorCount >= kThemeMaxColors)
        return;
    Entry *e = &s_state.colors[s_state.colorCount++];
    size_t kl = strlen(key);
    if (kl > sizeof(e->name) - 1)
        kl = sizeof(e->name) - 1;
    memcpy(e->name, key, kl);
    e->name[kl] = 0;
    e->a = r;
    e->b = g;
    e->c = b;
}

    /// @brief Store (or replace) a named metric.
    /// @param key Metric key.
    /// @param v Metric value.
    static void StoreMetric(const char *key, float v)
{
    for (int i = 0; i < s_state.metricCount; i++) {
        if (strcmp(s_state.metrics[i].name, key) == 0) {
            s_state.metrics[i].a = v;
            return;
        }
    }
    if (s_state.metricCount >= kThemeMaxMetrics)
        return;
    Entry *e = &s_state.metrics[s_state.metricCount++];
    size_t kl = strlen(key);
    if (kl > sizeof(e->name) - 1)
        kl = sizeof(e->name) - 1;
    memcpy(e->name, key, kl);
    e->name[kl] = 0;
    e->a = v;
    e->b = 0.0f;
    e->c = 0.0f;
}

    static void ApplyDefaults(bool dark, float inkRed, float inkGreen, float inkBlue);
    static void ApplyRowCustom(const ThemeRow *row);

    /// @brief Apply a theme table row to the live color set.
    /// @param idx Theme table index.
    static void ApplyRow(int idx)
{
    const ThemeRow *t = &s_state.themes[idx];
    s_state.colorCount = 0;
    for (int i = 0; i < kThemeColumnCount; i++)
        StoreColor(kThemeColumnKeys[i], t->col[i][0], t->col[i][1], t->col[i][2]);
    
    StoreColor("bg", t->col[0][0], t->col[0][1], t->col[0][2]);
    StoreColor("panel", t->col[5][0], t->col[5][1], t->col[5][2]);
    StoreColor("dock", t->col[5][0], t->col[5][1], t->col[5][2]);
    StoreColor("ink", t->ink[0], t->ink[1], t->ink[2]);
    StoreColor("dim", t->col[6][0], t->col[6][1], t->col[6][2]);
    ApplyDefaults(t->dark, t->ink[0], t->ink[1], t->ink[2]);
    ApplyRowCustom(t);
    size_t nl = strlen(t->name);
    if (nl > sizeof(s_state.activeName) - 1)
        nl = sizeof(s_state.activeName) - 1;
    memcpy(s_state.activeName, t->name, nl);
    s_state.activeName[nl] = 0;
}

    constexpr int kThemeCustomMaxMenus = 8;
    constexpr int kThemeCustomMaxColors = 16;
    constexpr int kThemeDefaultMaxColors = 48;

    /// @brief Shared fallback colors from the theme table defaults section.
    struct DefaultEntry {
        char key[24];
        float red;
        float green;
        float blue;
    };

    static DefaultEntry s_defaults[kThemeDefaultMaxColors];
    static int s_defaultCount = 0;

    /// @brief Retain the active theme file bytes.
    /// @param fileData File bytes.
    /// @param fileSize Byte size.
    static void RetainThemeFile(const char *fileData, unsigned fileSize)
    {
        free(s_state.themeFileData);
        s_state.themeFileData = nullptr;
        s_state.themeFileSize = 0;

        if (!fileData || fileSize == 0 || fileSize > 65536) {
            return;
        }

        char *fileCopy = (char *)malloc(fileSize + 1);

        if (!fileCopy) {
            return;
        }

        memcpy(fileCopy, fileData, fileSize);
        fileCopy[fileSize] = 0;
        s_state.themeFileData = fileCopy;
        s_state.themeFileSize = fileSize;
    }

    /// @brief Whether a custom section name is usable.
    /// @param sectionName Lowercase name.
    /// @return True when valid.
    static bool IsCustomNameUsable(const char *sectionName)
    {
        if (!sectionName || !sectionName[0]) {
            return false;
        }

        for (const char *cursor = sectionName; *cursor; cursor++) {
            char current = *cursor;
            bool lower = current >= 'a' && current <= 'z';
            bool digit = current >= '0' && current <= '9';

            if (!lower && !digit && current != '_') {
                return false;
            }
        }

        return true;
    }

    /// @brief Apply namespaced custom submenu colors from a theme file.
    /// @param fileData File bytes.
    /// @param fileSize Byte size.
    static void ApplyCustomColors(const char *fileData, unsigned fileSize)
    {
        JxDoc customDoc;

        if (!fileData || fileSize == 0 || !Jx::Parse(customDoc, fileData, fileSize)) {
            return;
        }

        if (Jx::TokType(customDoc, 0) != 1) {
            return;
        }

        int customRoot = Jx::ObjGet(customDoc, 0, "custom");

        if (Jx::TokType(customDoc, customRoot) != 1) {
            return;
        }

        int menuSpan = Jx::TokAfter(customDoc, customRoot);
        int menuCount = 0;

        for (int menuToken = customRoot + 1; menuToken < menuSpan && menuCount < kThemeCustomMaxMenus;) {
            char menuName[32] = { 0 };

            if (Jx::TokType(customDoc, menuToken) != 3) {
                break;
            }

            Jx::Str(customDoc, menuToken, menuName, sizeof(menuName));

            if (!IsCustomNameUsable(menuName) || Jx::TokType(customDoc, menuToken + 1) != 1) {
                menuToken = Jx::TokAfter(customDoc, menuToken + 1);
                continue;
            }

            int colorSpan = Jx::TokAfter(customDoc, menuToken + 1);
            int colorCount = 0;

            for (int colorToken = menuToken + 2; colorToken < colorSpan && colorCount < kThemeCustomMaxColors;) {
                char colorKey[24] = { 0 };

                if (Jx::TokType(customDoc, colorToken) != 3) {
                    break;
                }

                Jx::Str(customDoc, colorToken, colorKey, sizeof(colorKey));

                char colorValue[16] = { 0 };
                Jx::Str(customDoc, colorToken + 1, colorValue, sizeof(colorValue));

                if (IsCustomNameUsable(colorKey)) {
                    float red = 1.0f;
                    float green = 0.0f;
                    float blue = 1.0f;

                    if (ParseHex(colorValue, &red, &green, &blue)) {
                        char namespacedKey[24] = { 0 };
                        snprintf(namespacedKey, sizeof(namespacedKey), "%s_%s", menuName, colorKey);
                        StoreColor(namespacedKey, red, green, blue);
                        colorCount++;
                    }
                }

                colorToken = Jx::TokAfter(customDoc, colorToken + 1);
            }

            menuCount++;
            menuToken = Jx::TokAfter(customDoc, menuToken + 1);
        }
    }

    /// @brief Whether a color key is structural (columns or derived aliases).
    /// Structural keys always come from the table row, never overrides.
    /// @param key Color key.
    /// @return True when structural.
    static bool IsStructuralKey(const char *key)
    {
        if (!key)
            return true;

        for (int i = 0; i < kThemeColumnCount; i++) {
            if (strcmp(key, kThemeColumnKeys[i]) == 0)
                return true;
        }

        return strcmp(key, "bg") == 0 || strcmp(key, "panel") == 0 || strcmp(key, "dock") == 0 ||
               strcmp(key, "ink") == 0 || strcmp(key, "dim") == 0;
    }

    /// @brief Store one shared default color (structural keys rejected).
    /// @param key Color key.
    /// @param red Red 0-1.
    /// @param green Green 0-1.
    /// @param blue Blue 0-1.
    static void StoreDefault(const char *key, float red, float green, float blue)
    {
        if (!IsCustomNameUsable(key) || IsStructuralKey(key))
            return;

        for (int i = 0; i < s_defaultCount; i++) {
            if (strcmp(s_defaults[i].key, key) == 0) {
                s_defaults[i].red = red;
                s_defaults[i].green = green;
                s_defaults[i].blue = blue;
                return;
            }
        }

        if (s_defaultCount >= kThemeDefaultMaxColors)
            return;

        DefaultEntry *entry = &s_defaults[s_defaultCount++];
        size_t keyLength = strlen(key);

        if (keyLength > sizeof(entry->key) - 1)
            keyLength = sizeof(entry->key) - 1;

        memcpy(entry->key, key, keyLength);
        entry->key[keyLength] = 0;
        entry->red = red;
        entry->green = green;
        entry->blue = blue;
    }

    /// @brief Apply the shared defaults plus dark/light picks.
    /// @param dark True for the dark variants.
    /// @param inkRed Row ink red (light icon fallback).
    /// @param inkGreen Row ink green.
    /// @param inkBlue Row ink blue.
    static void ApplyDefaults(bool dark, float inkRed, float inkGreen, float inkBlue)
    {
        for (int i = 0; i < s_defaultCount; i++)
            StoreColor(s_defaults[i].key, s_defaults[i].red, s_defaults[i].green, s_defaults[i].blue);

        float batteryRed = 0.0f;
        float batteryGreen = 0.0f;
        float batteryBlue = 0.0f;

        if (dark) {
            if (!TryColor("battery_dark", &batteryRed, &batteryGreen, &batteryBlue)) {
                batteryRed = 0.8157f;
                batteryGreen = 0.8157f;
                batteryBlue = 0.8471f;
            }
        } else if (!TryColor("battery_light", &batteryRed, &batteryGreen, &batteryBlue)) {
            batteryRed = 0.3333f;
            batteryGreen = 0.3333f;
            batteryBlue = 0.3608f;
        }

        StoreColor("battery", batteryRed, batteryGreen, batteryBlue);

        float iconRed = 0.0f;
        float iconGreen = 0.0f;
        float iconBlue = 0.0f;

        if (dark) {
            if (!TryColor("icon_dark", &iconRed, &iconGreen, &iconBlue)) {
                iconRed = 1.0f;
                iconGreen = 1.0f;
                iconBlue = 1.0f;
            }
        } else if (!TryColor("icon_light", &iconRed, &iconGreen, &iconBlue)) {
            iconRed = inkRed;
            iconGreen = inkGreen;
            iconBlue = inkBlue;
        }

        StoreColor("icon", iconRed, iconGreen, iconBlue);
    }

    /// @brief Apply one row's custom colors over the defaults.
    /// @param row Theme row.
    static void ApplyRowCustom(const ThemeRow *row)
    {
        for (int i = 0; i < row->customCount; i++)
            StoreColor(row->customKey[i], row->customValue[i][0], row->customValue[i][1], row->customValue[i][2]);
    }

    /// @brief Load the packed theme table JSON.
    /// @param json JSON text.
    /// @param len Text length.
    /// @return True when at least one theme loaded.
    static bool LoadTable(const char *json, unsigned len)
{
    JxDoc doc;
    if (!Jx::Parse(doc, json, len))
        return false;
    if (Jx::TokType(doc, 0) != 1)
        return false;
    s_defaultCount = 0;
    int defaultsRoot = Jx::ObjGet(doc, 0, "defaults");

    if (Jx::TokType(doc, defaultsRoot) == 1) {
        int defaultsSpan = Jx::TokAfter(doc, defaultsRoot);

        for (int token = defaultsRoot + 1; token < defaultsSpan;) {
            char defaultKey[24] = { 0 };

            if (Jx::TokType(doc, token) != 3)
                break;

            Jx::Str(doc, token, defaultKey, sizeof(defaultKey));

            char defaultValue[16] = { 0 };
            Jx::Str(doc, token + 1, defaultValue, sizeof(defaultValue));

            float red = 1.0f;
            float green = 0.0f;
            float blue = 1.0f;

            if (ParseHex(defaultValue, &red, &green, &blue))
                StoreDefault(defaultKey, red, green, blue);

            token = Jx::TokAfter(doc, token + 1);
        }
    }

    int ta = Jx::ObjGet(doc, 0, "t");
    if (Jx::TokType(doc, ta) != 2)
        return false;
    int n = Jx::ArrSize(doc, ta);
    s_state.themeCount = 0;
    for (int i = 0; i < n && s_state.themeCount < kThemeMaxThemes; i++) {
        int row = Jx::ArrAt(doc, ta, i);
        if (Jx::TokType(doc, row) != 2 || Jx::ArrSize(doc, row) < 16)
            continue;
        ThemeRow *t = &s_state.themes[s_state.themeCount];
        memset(t, 0, sizeof(*t));
        Jx::Str(doc, Jx::ArrAt(doc, row, 0), t->name, sizeof(t->name));
        t->dark = Jx::Num(doc, Jx::ArrAt(doc, row, 1), 0) != 0;
        char hb[16];
        for (int c = 0; c < kThemeColumnCount; c++) {
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
        if (Jx::ArrSize(doc, row) > 16 && Jx::TokType(doc, Jx::ArrAt(doc, row, 16)) == 1) {
            int customRoot = Jx::ArrAt(doc, row, 16);
            int customSpan = Jx::TokAfter(doc, customRoot);

            for (int token = customRoot + 1; token < customSpan && t->customCount < kThemeCustomColors;) {
                char customKey[24] = { 0 };

                if (Jx::TokType(doc, token) != 3)
                    break;

                Jx::Str(doc, token, customKey, sizeof(customKey));

                char customValue[16] = { 0 };
                Jx::Str(doc, token + 1, customValue, sizeof(customValue));

                float red = 1.0f;
                float green = 0.0f;
                float blue = 1.0f;

                if (IsCustomNameUsable(customKey) && !IsStructuralKey(customKey) &&
                    ParseHex(customValue, &red, &green, &blue)) {
                    memcpy(t->customKey[t->customCount], customKey, sizeof(t->customKey[0]));
                    t->customValue[t->customCount][0] = red;
                    t->customValue[t->customCount][1] = green;
                    t->customValue[t->customCount][2] = blue;
                    t->customCount++;
                }

                token = Jx::TokAfter(doc, token + 1);
            }
        }
        s_state.themeCount++;
    }
    return s_state.themeCount > 0;
}

    /// @brief Load a legacy single-theme JSON blob.
    /// @param json JSON text.
    /// @param len Text length.
    /// @return True on success.
    bool LoadJson(const char *json, unsigned len)
{
    JxDoc doc;
    if (!Jx::Parse(doc, json, len))
        return false;
    if (Jx::TokType(doc, 0) != 1)
        return false;
    s_state.colorCount = 0;
    s_state.metricCount = 0;
    s_state.activeName[0] = 0;
    Jx::Str(doc, Jx::ObjGet(doc, 0, "name"), s_state.activeName, sizeof(s_state.activeName));
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
    ApplyCustomColors(json, len);
    RetainThemeFile(json, len);
    return true;
}

    /// @brief Look up a theme color.
    /// @param key Color key (for example "ink").
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    void Color(const char *key, float *r, float *g, float *b)
{
    for (int i = 0; i < s_state.colorCount; i++) {
        if (strcmp(s_state.colors[i].name, key) == 0) {
            *r = s_state.colors[i].a;
            *g = s_state.colors[i].b;
            *b = s_state.colors[i].c;
            return;
        }
    }
    *r = 1.0f;
    *g = 0.0f;
    *b = 1.0f;
}

    /// @brief Look up a theme color without the magenta fallback.
    /// @param key Color key.
    /// @param r Red output (untouched when missing).
    /// @param g Green output (untouched when missing).
    /// @param b Blue output (untouched when missing).
    /// @return True when found.
    bool TryColor(const char *key, float *r, float *g, float *b)
{
    for (int i = 0; i < s_state.colorCount; i++) {
        if (strcmp(s_state.colors[i].name, key) == 0) {
            *r = s_state.colors[i].a;
            *g = s_state.colors[i].b;
            *b = s_state.colors[i].c;
            return true;
        }
    }
    return false;
}

    /// @brief Look up a theme metric.
    /// @param key Metric key (for example "corner").
    /// @param dflt Default when missing.
    /// @return Metric value or default.
    float Metric(const char *key, float dflt)
{
    for (int i = 0; i < s_state.metricCount; i++) {
        if (strcmp(s_state.metrics[i].name, key) == 0)
            return s_state.metrics[i].a;
    }
    return dflt;
}

    /// @brief Give the active theme name.
    /// @return Theme name.
    const char *Name()
{
    return s_state.activeName;
}

    /// @brief Number of themes in the table.
    /// @return Theme count.
    int ThemeCount()
{
    return s_state.themeCount;
}

    /// @brief Look up a theme name by index.
    /// @param i Theme index.
    /// @return Theme name, or "" when out of range.
    const char *ThemeName(int i)
{
    if (i < 0 || i >= s_state.themeCount)
        return "";
    return s_state.themes[i].name;
}

    /// @brief Give the active theme index.
    /// @return Theme index.
    int GetTheme()
{
    return s_state.activeTheme;
}

    /// @brief Whether the active theme is dark.
    /// @return True for dark themes.
    bool IsDark()
{
    return s_state.activeTheme >= 0 && s_state.activeTheme < s_state.themeCount && s_state.themes[s_state.activeTheme].dark;
}

    /// @brief Give a tab's background color.
    /// @param menu Tab index.
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    void MenuColor(int menu, float *r, float *g, float *b)
{
    if (s_state.activeTheme < 0 || s_state.activeTheme >= s_state.themeCount || menu < 0 || menu > 4) {
        *r = 1.0f;
        *g = 1.0f;
        *b = 1.0f;
        return;
    }
    *r = s_state.themes[s_state.activeTheme].col[menu][0];
    *g = s_state.themes[s_state.activeTheme].col[menu][1];
    *b = s_state.themes[s_state.activeTheme].col[menu][2];
}

    /// @brief Give a tab's accent color.
    /// @param menu Tab index.
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    void MenuAccent(int menu, float *r, float *g, float *b)
{
    if (s_state.activeTheme < 0 || s_state.activeTheme >= s_state.themeCount || menu < 0 || menu > 4) {
        *r = 0.5f;
        *g = 0.5f;
        *b = 0.5f;
        return;
    }
    *r = s_state.themes[s_state.activeTheme].col[8 + menu][0];
    *g = s_state.themes[s_state.activeTheme].col[8 + menu][1];
    *b = s_state.themes[s_state.activeTheme].col[8 + menu][2];
}

    /// @brief Give a theme's preview swatch color.
    /// @param t Theme index.
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    void ThemePreview(int t, float *r, float *g, float *b)
{
    if (t < 0 || t >= s_state.themeCount) {
        *r = 0.5f;
        *g = 0.5f;
        *b = 0.5f;
        return;
    }
    *r = s_state.themes[t].col[0][0];
    *g = s_state.themes[t].col[0][1];
    *b = s_state.themes[t].col[0][2];
}

    /// @brief Persist the active theme index, preserving custom colors.
    static void PersistTheme()
    {
        char retainedCustom[65536] = { 0 };
        unsigned retainedCustomSize = 0;

        if (s_state.themeFileData && s_state.themeFileSize > 0) {
            JxDoc retainedDoc;

            if (Jx::Parse(retainedDoc, s_state.themeFileData, s_state.themeFileSize) &&
                Jx::TokType(retainedDoc, 0) == 1) {
                int customRoot = Jx::ObjGet(retainedDoc, 0, "custom");

                if (Jx::TokType(retainedDoc, customRoot) == 1 &&
                    customRoot >= 0 && customRoot < retainedDoc.count) {
                    int customStart = retainedDoc.toks[customRoot].start;
                    int customEnd = retainedDoc.toks[customRoot].end;

                    if (customStart >= 0 && customEnd > customStart &&
                        (unsigned)customEnd <= s_state.themeFileSize &&
                        (unsigned)(customEnd - customStart) < sizeof(retainedCustom)) {
                        memcpy(retainedCustom, s_state.themeFileData + customStart,
                               (size_t)(customEnd - customStart));
                        retainedCustomSize = (unsigned)(customEnd - customStart);
                    }
                }
            }
        }

        FILE *themeFile = fopen("sdmc:/qlaunch-ext/theme.json", "wb");

        if (!themeFile) {
            return;
        }

        if (retainedCustomSize > 0) {
            fprintf(themeFile, "{\"theme\": %d,\n\"custom\": %.*s}\n", s_state.activeTheme,
                    (int)retainedCustomSize, retainedCustom);
        } else {
            char indexBuffer[48];
            snprintf(indexBuffer, sizeof(indexBuffer), "{\"theme\": %d}", s_state.activeTheme);
            fwrite(indexBuffer, 1, strlen(indexBuffer), themeFile);
        }

        fclose(themeFile);
    }

    /// @brief Activate a theme by index.
    /// @param i Theme index.
    /// @return True on success.
    bool SetTheme(int i)
{
    if (i < 0 || i >= s_state.themeCount)
        return false;
    if (!s_state.metricCount) {
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
    s_state.activeTheme = i;
    ApplyRow(i);
    if (Gfx::Ready()) {
        Icons::ResetHud();
        Icons::ReapplyPack();
    }
    PersistTheme();
    return true;
}

    /// @brief Load a builtin theme by name.
    /// @param name "light" or "dark".
    /// @return True on success.
    bool LoadBuiltin(const char *name)
{
    if (!s_state.themeCount &&
        !LoadTable(themes_json, themes_json_size))
        return LoadJson(
            (name && strcmp(name, "dark") == 0) ? theme_dark_json : theme_light_json,
            (name && strcmp(name, "dark") == 0) ? theme_dark_json_size : theme_light_json_size);
    bool builtinOk = false;

    if (name && strcmp(name, "dark") == 0) {
        builtinOk = SetTheme(1);
    } else {
        builtinOk = SetTheme(0);
    }

    if (builtinOk) {
        RetainThemeFile(themes_json, themes_json_size);
    }

    return builtinOk;
}

    /// @brief Load a theme override from an SD file.
    /// @param path Filesystem path (64KB max).
    /// @return True on success.
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
            if (!s_state.themeCount)
                LoadTable(themes_json, themes_json_size);

            int themeIndex = (int)Jx::Num(doc, tt, 0);

            if (themeIndex >= 0 && themeIndex < s_state.themeCount) {
                RetainThemeFile(buf, (unsigned)got);
                ok = SetTheme(themeIndex);

                if (ok) {
                    ApplyCustomColors(buf, (unsigned)got);
                }
            }

            free(buf);
            return ok;
        }
    }
    ok = LoadJson(buf, (unsigned)got);
    free(buf);
    return ok;
}

    /// @brief Load the theme table from the pack, else the embedded one.
    /// @param packDir Theme pack directory (may be null).
    /// @return True when a table with rows is active.
    bool LoadPackTable(const char *packDir)
    {
        if (packDir && packDir[0]) {
            char tablePath[256];
            snprintf(tablePath, sizeof(tablePath), "%s/themes.json", packDir);
            tablePath[sizeof(tablePath) - 1] = 0;

            unsigned tableSize = 0;
            char *tableData = (char *)Packs::ReadFile(tablePath, 65536, &tableSize);

            if (tableData) {
                bool tableOk = LoadTable(tableData, tableSize);

                if (tableOk) {
                    RetainThemeFile(tableData, tableSize);
                }

                free(tableData);

                if (tableOk) {
                    return true;
                }
            }
        }

        if (!s_state.themeCount) {
            LoadTable(themes_json, themes_json_size);
        }

        return s_state.themeCount > 0;
    }

    /// @brief Load the pack theme table, then the persisted index, else the system set.
    /// @param packDir Theme pack directory (may be null).
    /// @return True on success.
    bool InitAuto(const char *packDir)
{
    if (!LoadPackTable(packDir))
        return false;
    if (LoadFile("sdmc:/qlaunch-ext/theme.json"))
        return true;

    bool autoOk = false;

    if (qext_colorset() == ColorSetId_Dark) {
        autoOk = SetTheme(s_state.themeCount > 1 ? 1 : 0);
    } else {
        autoOk = SetTheme(0);
    }

    if (autoOk) {
        RetainThemeFile(themes_json, themes_json_size);
    }

    return autoOk;
}

    /// @brief Shut down theme state and free retained file bytes.
    void Shutdown()
    {
        free(s_state.themeFileData);
        s_state.themeFileData = nullptr;
        s_state.themeFileSize = 0;
    }

    /// @brief Give the current theme file bytes.
    /// @param outputData Output file pointer, valid until the next theme change.
    /// @param outputSize Output byte size.
    /// @return True when a theme file is retained.
    bool ThemeFile(const unsigned char **outputData, unsigned *outputSize)
    {
        if (!outputData || !outputSize) {
            return false;
        }

        if (!s_state.themeFileData || s_state.themeFileSize == 0) {
            return false;
        }

        *outputData = (const unsigned char *)s_state.themeFileData;
        *outputSize = s_state.themeFileSize;
        return true;
    }

    /// @brief Resolve a tab id to a theme background slot.
    /// @param tabId Tab id (for example "album").
    /// @return 0-4 for known tabs, or -1 when unknown.
    int MenuSlotForId(const char *tabId)
    {
        if (!tabId) {
            return -1;
        }

        if (strcmp(tabId, "home") == 0) {
            return 0;
        }

        if (strcmp(tabId, "connect") == 0) {
            return 1;
        }

        if (strcmp(tabId, "eshop") == 0) {
            return 2;
        }

        if (strcmp(tabId, "album") == 0) {
            return 3;
        }

        if (strcmp(tabId, "settings") == 0) {
            return 4;
        }

        return -1;
    }

} /* namespace Theme */