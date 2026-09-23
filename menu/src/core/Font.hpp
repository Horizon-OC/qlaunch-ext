/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

enum { FONT_ATLAS_W = 1024, FONT_ATLAS_H = 1024 };

/* Texture slots / text buffers. */
enum { FONT_SLOT_LATIN = 0, FONT_SLOT_KANA = 1, FONT_SLOT_ICON = 2 };
enum { ICONS_FIRST_TEX = 3 };

namespace Font {
bool Init();
void Shutdown();
void Refresh();
bool Ok();
float Draw(const char *s, float x, float y, float px,
           float r, float g, float b);
float Measure(const char *s, float px);
void Centered(const char *s, float cx, float y, float px, float maxW,
              float r, float g, float b);
unsigned Next(const char **pp);
} /* namespace Font */

