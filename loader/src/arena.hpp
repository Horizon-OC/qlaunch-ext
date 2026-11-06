/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering...         */

#pragma once
#include <stddef.h>
#include <stdint.h>

namespace arena {

struct Free { size_t size; Free *next; };
struct Head { size_t size; size_t magic; };   /* just before the payload, block starts here */
constexpr size_t kMagic = 0x4152454E41424C4BULL;
constexpr size_t kMinBlock = sizeof(Head) + 16;

extern Free *g_free;   /* sorted by address */

void Insert(char *start, size_t size);
void *Alloc(size_t n, size_t align);
size_t Usable(void *p);
void Release(void *p);
size_t FreeBytes();
/* If the last free block runs up to `end`, take it off the list and return where
   it starts; otherwise return `end`. That is how far the range can shrink. */
char *TrimEnd(char *end);
/* Drop free blocks at/above `end` after a heap shrink, clamping a block that
   spans `end` so the free list can never hand out unmapped pointers. */
void PruneAbove(char *end);

} // namespace arena
