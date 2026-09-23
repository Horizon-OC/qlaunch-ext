/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

namespace Theme {
bool LoadJson(const char *json, unsigned len);
bool LoadBuiltin(const char *name); /* "light" / "dark" */
bool LoadFile(const char *path);    /* SD override, 64KB max */
bool InitAuto();                    /* SD override, else system set */
void Color(const char *key, float *r, float *g, float *b);
float Metric(const char *key, float dflt);
const char *Name();
} /* namespace Theme */

