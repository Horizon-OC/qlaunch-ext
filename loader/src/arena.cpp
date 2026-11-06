/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering...         */

#include "arena.hpp"

namespace arena {

Free *g_free = nullptr;   /* sorted by address */

void Insert(char *start, size_t size)
{
    Free *prev = nullptr, *cur = g_free;
    while (cur && (char *)cur < start) {
        prev = cur;
        cur = cur->next;
    }
    Free *f = (Free *)start;
    f->size = size;
    f->next = cur;
    if (cur && start + size == (char *)cur) {
        f->size += cur->size;
        f->next = cur->next;
    }
    if (prev && (char *)prev + prev->size == start) {
        prev->size += f->size;
        prev->next = f->next;
    } else if (prev) {
        prev->next = f;
    } else {
        g_free = f;
    }
}

void *Alloc(size_t n, size_t align)
{
    if (align < 16)
        align = 16;
    if (n > SIZE_MAX / 2)
        return nullptr;
    n = (n + 15) & ~(size_t)15;

    /* Best fit: carve out of the smallest free block that satisfies the
       request, so long churn (icons, wavs, SDL queue chunks) does not
       fragment the arena into holes too small for anything useful. */
    Free *best = nullptr;
    Free *bestPrev = nullptr;
    char *bestP = nullptr;
    char *bestEnd = nullptr;
    char *bestStart = nullptr;
    char *bestLimit = nullptr;

    Free *prev = nullptr;
    for (Free *f = g_free; f; prev = f, f = f->next) {
        char *start = (char *)f;
        char *limit = start + f->size;
        char *p = (char *)(((uintptr_t)start + sizeof(Head) + align - 1) & ~(uintptr_t)(align - 1));
        /* A gap in front must be able to hold a free block of its own. */
        if (p - sizeof(Head) != start && (size_t)(p - sizeof(Head) - start) < kMinBlock)
            p += align;
        char *end = p + n;
        if (end > limit)
            continue;
        if ((size_t)(limit - end) < kMinBlock)
            end = limit;   /* a tail too small to track stays with this block */
        if (!best || f->size < best->size) {
            best = f;
            bestPrev = prev;
            bestP = p;
            bestEnd = end;
            bestStart = start;
            bestLimit = limit;
        }
    }

    if (!best)
        return nullptr;

    Free *next = best->next;
    if (bestPrev)
        bestPrev->next = next;
    else
        g_free = next;

    char *block = bestP - sizeof(Head);
    if (block != bestStart)
        Insert(bestStart, (size_t)(block - bestStart));
    if (bestEnd != bestLimit)
        Insert(bestEnd, (size_t)(bestLimit - bestEnd));

    Head *h = (Head *)block;
    h->size = (size_t)(bestEnd - block);
    h->magic = kMagic;
    return bestP;
}

size_t Usable(void *p)
{
    Head *h = (Head *)p - 1;
    return h->size - sizeof(Head);
}

void Release(void *p)
{
    Head *h = (Head *)p - 1;
    h->magic = 0;
    Insert((char *)h, h->size);
}

size_t FreeBytes()
{
    size_t n = 0;
    for (Free *f = g_free; f; f = f->next)
        n += f->size;
    return n;
}

char *TrimEnd(char *end)
{
    Free *prev = nullptr;
    for (Free *f = g_free; f; prev = f, f = f->next) {
        if ((char *)f + f->size == end) {
            if (prev)
                prev->next = f->next;
            else
                g_free = f->next;
            return (char *)f;
        }
    }
    return end;
}

void PruneAbove(char *end)
{
    Free *prev = nullptr;
    for (Free *f = g_free; f; ) {
        char *start = (char *)f;
        Free *next = f->next;
        if (start >= end) {
            if (prev)
                prev->next = next;
            else
                g_free = next;
        } else if (start + f->size > end) {
            f->size = (size_t)(end - start);
            prev = f;
        } else {
            prev = f;
        }
        f = next;
    }
}

} // namespace arena
