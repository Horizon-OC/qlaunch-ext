/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

namespace album {

void Init();
void Exit();
int Refresh();
int Count();

int FileId(int index, CapsAlbumFileId *out);

int ThumbSize(int index);

int Thumb(int index, void *out, unsigned cap);

int ImageSize(int index);

int Image(int index, void *out, unsigned cap);

int Label(int index, char *out, unsigned cap);

} // namespace album