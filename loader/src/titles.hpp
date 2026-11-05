/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

namespace titles {

/* Combined titles file holding game order, custom names, and folders. */
extern const char kTitlesPath[];

int Refresh();
int Count();
u64 Id(int index);
/* Returns name length (<0 on error). */
int Name(int index, char *out, unsigned out_cap);
/* Display version string (<0 when unknown). */
int Version(int index, char *out, unsigned out_cap);
/* Publisher string (<0 when unknown). */
int Publisher(int index, char *out, unsigned out_cap);
/* JPEG icon byte size (<0 if none). */
int IconSize(int index);
/* Copies JPEG icon bytes. Returns bytes written (<0 on error). */
int Icon(int index, void *out, unsigned cap);

/* Is the gamecard present */
int Ejected(int index);
/* Whether a tid is a virtual (plugin-provided) title. */
int IsVirtual(u64 tid);
/* Rename any known title, virtual or not (empty or blank names rejected). */
int Rename(u64 tid, const char *name);
/* Read a title's original name: NACP name, or live provider name when virtual. */
int OriginalName(int index, char *out, unsigned out_cap);
void NoteLaunched(u64 tid);
int TitlesChanged();

/* Persist game order, custom names, and the folders section. */
void Save();

} // namespace titles
