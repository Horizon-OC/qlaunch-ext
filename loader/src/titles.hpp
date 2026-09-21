/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

namespace titles {

int Refresh();
int Count();
u64 Id(int index);
/* Returns name length (<0 on error). */
int Name(int index, char *out, unsigned out_cap);
/* JPEG icon byte size (<0 if none). */
int IconSize(int index);
/* Copies JPEG icon bytes. Returns bytes written (<0 on error). */
int Icon(int index, void *out, unsigned cap);

} // namespace titles
