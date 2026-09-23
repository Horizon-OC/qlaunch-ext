/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Json.hpp"
#include <string.h>
#include <stdlib.h>

#include "../third_party/jsmn.h"

namespace Jx {

static int SkipTok(const JxTok *toks, int count, int t)
{
    if (t < 0 || t >= count)
        return count;
    int n = toks[t].size;
    if (n < 0)
        n = 0;
    int i = t + 1;
    for (int k = 0; k < n; k++) {
        if (i >= count)
            return count;
        i = SkipTok(toks, count, i);
    }
    return i > count ? count : i;
}

bool Parse(JxDoc &d, const char *text, unsigned len)
{
    d.text = text;
    d.count = 0;
    if (!text || !len || len > 0x100000)
        return false;
    jsmn_parser p;
    jsmn_init(&p);
    int r = jsmn_parse(&p, text, len, NULL, 0);
    if (r < 0 || r > JX_MAX_TOKS)
        return false;
    jsmntok_t *tmp = (jsmntok_t *)malloc(sizeof(jsmntok_t) * (unsigned)r);
    if (!tmp)
        return false;
    jsmn_init(&p);
    r = jsmn_parse(&p, text, len, tmp, r);
    if (r < 0) {
        free(tmp);
        return false;
    }
    for (int i = 0; i < r; i++) {
        int ty = 0;
        switch (tmp[i].type) {
        case JSMN_OBJECT: ty = 1; break;
        case JSMN_ARRAY: ty = 2; break;
        case JSMN_STRING: ty = 3; break;
        case JSMN_PRIMITIVE: ty = 4; break;
        default: ty = 0; break;
        }
        d.toks[i].type = ty;
        d.toks[i].start = tmp[i].start;
        d.toks[i].end = tmp[i].end;
        d.toks[i].size = tmp[i].size;
    }
    d.count = r;
    free(tmp);
    return d.count > 0;
}

int TokType(const JxDoc &d, int t)
{
    if (t < 0 || t >= d.count)
        return 0;
    return d.toks[t].type;
}

int TokAfter(const JxDoc &d, int t)
{
    if (t < 0 || t >= d.count)
        return t + 1;
    return SkipTok(d.toks, d.count, t);
}

int ObjGet(const JxDoc &d, int obj, const char *key)
{
    if (obj < 0 || obj >= d.count || d.toks[obj].type != 1 || !key)
        return -1;
    size_t klen = strlen(key);
    int j = obj + 1;
    for (int n = 0; n < d.toks[obj].size; n++) {
        int kt = j, vt = j + 1;
        if (kt < 0 || vt >= d.count)
            break;
        if (d.toks[kt].type == 3 &&
            (d.toks[kt].end - d.toks[kt].start) == (int)klen &&
            memcmp(d.text + d.toks[kt].start, key, klen) == 0)
            return vt;
        j = SkipTok(d.toks, d.count, vt);
    }
    return -1;
}

int ArrSize(const JxDoc &d, int arr)
{
    if (arr < 0 || arr >= d.count || d.toks[arr].type != 2)
        return 0;
    return d.toks[arr].size;
}

int ArrAt(const JxDoc &d, int arr, int i)
{
    if (arr < 0 || arr >= d.count || d.toks[arr].type != 2)
        return -1;
    if (i < 0 || i >= d.toks[arr].size)
        return -1;
    int j = arr + 1;
    for (int k = 0; k < i; k++) {
        if (j >= d.count)
            return -1;
        j = SkipTok(d.toks, d.count, j);
    }
    return (j < d.count) ? j : -1;
}

bool IsTrue(const JxDoc &d, int t)
{
    if (t < 0 || t >= d.count || d.toks[t].type != 4)
        return false;
    return (d.toks[t].end - d.toks[t].start) == 4 &&
           memcmp(d.text + d.toks[t].start, "true", 4) == 0;
}

long Num(const JxDoc &d, int t, long dflt)
{
    if (t < 0 || t >= d.count || d.toks[t].type != 4)
        return dflt;
    int len = d.toks[t].end - d.toks[t].start;
    if (len <= 0 || len > 20)
        return dflt;
    char buf[24];
    memcpy(buf, d.text + d.toks[t].start, (unsigned)len);
    buf[len] = 0;
    return strtol(buf, NULL, 10);
}

static void EmitUtf8(char *out, unsigned *pos, unsigned cap, unsigned cp)
{
    if (cp < 0x80) {
        if (*pos + 1 < cap)
            out[(*pos)++] = (char)cp;
    } else if (cp < 0x800) {
        if (*pos + 2 < cap) {
            out[(*pos)++] = (char)(0xC0 | (cp >> 6));
            out[(*pos)++] = (char)(0x80 | (cp & 0x3F));
        }
    } else {
        if (*pos + 1 < cap)
            out[(*pos)++] = (char)63;
    }
}

int Str(const JxDoc &d, int t, char *out, unsigned cap)
{
    if (t < 0 || t >= d.count || !out || !cap)
        return -1;
    if (d.toks[t].type != 3 && d.toks[t].type != 4)
        return -1;
    unsigned pos = 0;
    for (int i = d.toks[t].start; i < d.toks[t].end && pos + 1 < cap; i++) {
        char c = d.text[i];
        if (c == 92 && i + 1 < d.toks[t].end) {
            char e = d.text[++i];
            if (e == 110)
                EmitUtf8(out, &pos, cap, 10);
            else if (e == 114)
                EmitUtf8(out, &pos, cap, 13);
            else if (e == 116)
                EmitUtf8(out, &pos, cap, 9);
            else if (e == 98)
                EmitUtf8(out, &pos, cap, 8);
            else if (e == 102)
                EmitUtf8(out, &pos, cap, 12);
            else if (e == 117) {
                unsigned cp = 0;
                for (int k = 0; k < 4 && i + 1 < d.toks[t].end; k++) {
                    char h = d.text[++i];
                    cp <<= 4;
                    if (h >= 48 && h <= 57)
                        cp |= (unsigned)(h - 48);
                    else if (h >= 97 && h <= 102)
                        cp |= (unsigned)(h - 87);
                    else if (h >= 65 && h <= 70)
                        cp |= (unsigned)(h - 55);
                }
                EmitUtf8(out, &pos, cap, cp);
            } else
                EmitUtf8(out, &pos, cap, (unsigned char)e);
        } else {
            EmitUtf8(out, &pos, cap, (unsigned char)c);
        }
    }
    out[pos] = 0;
    return (int)pos;
}

bool StrEq(const JxDoc &d, int t, const char *s)
{
    if (t < 0 || t >= d.count || !s)
        return false;
    if (d.toks[t].type != 3)
        return false;
    size_t klen = strlen(s);
    return (d.toks[t].end - d.toks[t].start) == (int)klen &&
           memcmp(d.text + d.toks[t].start, s, klen) == 0;
}

} /* namespace Jx */

