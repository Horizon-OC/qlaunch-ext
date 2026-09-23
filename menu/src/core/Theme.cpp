/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Theme.hpp"
#include "Json.hpp"
#include "Qext.hpp"
#include "Assets.hpp"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

namespace Theme {

enum { MAX_COLORS = 64, MAX_METRICS = 32 };

struct Entry {
    char name[24];
    float a, b, c;
};

static char g_name[32];
static Entry g_colors[MAX_COLORS];
static int g_nColors;
static Entry g_metrics[MAX_METRICS];
static int g_nMetrics;

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

bool LoadBuiltin(const char *name)
{
    if (name && strcmp(name, "dark") == 0)
        return LoadJson(theme_dark_json, theme_dark_json_size);
    return LoadJson(theme_light_json, theme_light_json_size);
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
    char *buf = (char *)malloc((unsigned)sz);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t got = fread(buf, 1, (unsigned)sz, f);
    fclose(f);
    bool ok = (got == (unsigned)sz) && LoadJson(buf, (unsigned)sz);
    free(buf);
    return ok;
}

bool InitAuto()
{
    if (LoadFile("sdmc:/qlaunch-ext/theme.json"))
        return true;
    ColorSetId cs = ColorSetId_Light;
    if (R_SUCCEEDED(setsysGetColorSetId(&cs)) && cs == ColorSetId_Dark)
        return LoadBuiltin("dark");
    return LoadBuiltin("light");
}

} /* namespace Theme */

