/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <stddef.h>

struct JxTok {
    int type;  /* 0 undef, 1 obj, 2 arr, 3 str, 4 prim */
    int start;
    int end;
    int size;  /* children count  */
};

enum { JX_MAX_TOKS = 512 };

struct JxDoc {
    const char *text;
    JxTok toks[JX_MAX_TOKS];
    int count;
};

namespace Jx {
bool Parse(JxDoc &d, const char *text, unsigned len);
int ObjGet(const JxDoc &d, int obj, const char *key);
int TokType(const JxDoc &d, int t);
int ArrSize(const JxDoc &d, int arr);
int ArrAt(const JxDoc &d, int arr, int i);
int TokAfter(const JxDoc &d, int t);
bool IsTrue(const JxDoc &d, int t);
long Num(const JxDoc &d, int t, long dflt);
int Str(const JxDoc &d, int t, char *out, unsigned cap);
bool StrEq(const JxDoc &d, int t, const char *s);
} /* namespace Jx */

