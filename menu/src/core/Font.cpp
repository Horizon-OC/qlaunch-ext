/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Font.hpp"
#include "Gfx.hpp"
#include "Log.hpp"
#include "Packs.hpp"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define STBTT_assert(x)
#define STB_TRUETYPE_IMPLEMENTATION
#include "../third_party/stb_truetype.h"

namespace Font {

    struct Atlas {
        int first;
        int count;
        int slot; /* Allocated by engine */
        stbtt_bakedchar g[512];
        float px;
        int w;
        int h;
        bool ok;
    };

    /// @brief All mutable font atlas state, single global.
    struct FontState {
        Atlas atlases[3];
        bool isInitialized;
        const uint8_t *latinData;
        unsigned latinSize;
        const uint8_t *iconData;
        unsigned iconSize;
        uint8_t *latinOwned;
        uint8_t *iconOwned;
    };

    static FontState s_state = {};
    static constexpr unsigned kFontFileMax = 8 * 1024 * 1024;

        /// @brief Load a pack font file (pack-only, no embedded fallback).
        /// @param packDir Theme pack directory (may be null).
        /// @param fileName Pack asset name (for example "font.otf").
        /// @param sizeOut Output byte size (zero on failure).
        /// @return Owned bytes, or null when missing.
        static uint8_t *ResolveFontFile(const char *packDir, const char *fileName,
                                        unsigned *sizeOut)
        {
            *sizeOut = 0;

            if (!packDir || !packDir[0] || !fileName || !fileName[0]) {
                return nullptr;
            }

            unsigned packSize = 0;
            unsigned char *packData = Packs::ReadAsset(packDir, fileName, kFontFileMax, &packSize);

            if (!packData || packSize == 0) {
                free(packData);
                Log::Line("[menu] missing pack asset '%s/assets/%s'", packDir ? packDir : "?",
                           fileName ? fileName : "?");
                return nullptr;
            }

            *sizeOut = packSize;
            Log::Line("[menu] pack font '%s'", fileName);
            return packData;
        }

        struct AtlasSize {
            int w;
            int h;
        };

        /// @brief Bake one glyph range, dynamically allocating the smallest
        ///        scratch atlas that fits at the highest available quality.
        /// @param atlas Atlas receiving the bake.
        /// @param file Font file bytes.
        /// @param heights Pixel heights to try (descending quality).
        /// @param heightCount Height count.
        /// @param sizes Candidate atlas dimensions to try (ascending area).
        /// @param sizeCount Candidate count.
        /// @return True when baked and uploaded.
        static bool BakeAndUpload(Atlas *atlas, const uint8_t *file, const float *heights, int heightCount,
                                  const AtlasSize *sizes, int sizeCount)
        {
            for (int si = 0; si < sizeCount; si++) {
                int w = sizes[si].w;
                int h = sizes[si].h;
                if (w > kFontAtlasMaxWidth || h > kFontAtlasMaxHeight)
                    continue;

                for (int hi = 0; hi < heightCount; hi++) {
                    size_t bytes = (size_t)w * (size_t)h;
                    uint8_t *bitmap = (uint8_t *)malloc(bytes);
                    if (!bitmap)
                        return false;
                    memset(bitmap, 0, bytes);

                    int rows = stbtt_BakeFontBitmap(file, 0, heights[hi], bitmap, w, h,
                                                    atlas->first, atlas->count, atlas->g);

                    if (rows > 0 && rows <= h) {
                        atlas->px = heights[hi];
                        atlas->w = w;
                        atlas->h = h;
                        atlas->ok = Gfx::TexUpload(atlas->slot, bitmap, w, h, Gfx::TexFormat::R8);
                        free(bitmap);
                        if (atlas->ok) {
                            Log::Line("[menu] Bake atlas range 0x%X: %dx%d @ %.0fpx (%d rows)",
                                      (unsigned)atlas->first, w, h, heights[hi], rows);
                        }
                        return atlas->ok;
                    }
                    free(bitmap);
                }
            }
            atlas->px = 0.0f;
            atlas->w = 0;
            atlas->h = 0;
            return false;
        }

        /// @brief Shut down font rendering.
        void Shutdown()
        {
            if (!s_state.isInitialized)
                return;
            s_state.isInitialized = false;

            /* TODO: Dynamically load the atlases? */
            for (int i = 0; i < 3; i++) {
                s_state.atlases[i].ok = false;
                Gfx::TexFree(s_state.atlases[i].slot);
                s_state.atlases[i].slot = -1;
            }

            free(s_state.latinOwned);
            free(s_state.iconOwned);
            s_state.latinOwned = nullptr;
            s_state.iconOwned = nullptr;
            s_state.latinData = nullptr;
            s_state.iconData = nullptr;
        }

        /// @brief Give the engine texture slot for a text buffer.
        /// @param buf Text buffer id (0-2).
        /// @return Texture slot, or < 0 when unavailable.
        int Slot(int buf)
        {
            if (buf < 0 || buf > 2 || !s_state.atlases[buf].ok)
                return -1;
            return s_state.atlases[buf].slot;
        }

        /// @brief Initialize font atlases and text buffers.
        /// Pack fonts override the embedded ones when present.
        /// @param packDir Theme pack directory (may be null).
        /// @return True on success.
        bool Init(const char *packDir) {
            static const float big[] = { 64.0f, 56.0f, 48.0f, 40.0f };
            static const float sml[] = { 48.0f, 40.0f, 32.0f };
            /* Atlas size candidates, ascending area. Smaller textures reduce
               GPU/unified-memory pressure. */
            static const AtlasSize textSizes[] = {
                { 512, 256 }, { 512, 384 }, { 512, 512 },
                { 1024, 256 }, { 1024, 384 }, { 1024, 512 }, { 1024, 768 }, { 1024, 1024 },
            };
            static const AtlasSize iconSizes[] = {
                { 512, 512 }, { 1024, 256 }, { 1024, 384 }, { 1024, 512 },
                { 1024, 768 }, { 1024, 1024 },
            };

            s_state.atlases[0].first = 32;
            s_state.atlases[0].count = 224;
            s_state.atlases[0].slot = Gfx::TexAlloc();
            s_state.atlases[0].ok = false;
            s_state.atlases[0].w = 0;
            s_state.atlases[0].h = 0;

            s_state.atlases[1].first = 0x3040;
            s_state.atlases[1].count = 192;
            s_state.atlases[1].slot = Gfx::TexAlloc();
            s_state.atlases[1].ok = false;
            s_state.atlases[1].w = 0;
            s_state.atlases[1].h = 0;

            s_state.atlases[2].first = 0xE000;
            s_state.atlases[2].count = 512;
            s_state.atlases[2].slot = Gfx::TexAlloc();
            s_state.atlases[2].ok = false;
            s_state.atlases[2].w = 0;
            s_state.atlases[2].h = 0;

            if (s_state.atlases[0].slot < 0 || s_state.atlases[1].slot < 0 || s_state.atlases[2].slot < 0)
                return false;

            s_state.latinOwned = ResolveFontFile(packDir, "font.otf", &s_state.latinSize);
            s_state.iconOwned = ResolveFontFile(packDir, "switch_icons.ttf", &s_state.iconSize);
            s_state.latinData = s_state.latinOwned;
            s_state.iconData = s_state.iconOwned;

            if (!s_state.latinData || !s_state.iconData) {
                free(s_state.latinOwned);
                free(s_state.iconOwned);
                s_state.latinOwned = nullptr;
                s_state.iconOwned = nullptr;
                s_state.latinData = nullptr;
                s_state.iconData = nullptr;
                return false;
            }

            if (!BakeAndUpload(&s_state.atlases[0], s_state.latinData, big, 4,
                               textSizes, (int)(sizeof(textSizes) / sizeof(textSizes[0])))) {
                return false;
            }
            Log::Line("[menu] Latin atlas: %dx%d @ %.0fpx", s_state.atlases[0].w, s_state.atlases[0].h, s_state.atlases[0].px);

            if (BakeAndUpload(&s_state.atlases[1], s_state.latinData, big, 4,
                              textSizes, (int)(sizeof(textSizes) / sizeof(textSizes[0])))) {
                Log::Line("[menu] Kana atlas: %dx%d @ %.0fpx", s_state.atlases[1].w, s_state.atlases[1].h, s_state.atlases[1].px);
            }

            if (!BakeAndUpload(&s_state.atlases[2], s_state.iconData, sml, 3,
                               iconSizes, (int)(sizeof(iconSizes) / sizeof(iconSizes[0])))) {
                return false;
            }
            Log::Line("[menu] Icon atlas: %dx%d @ %.0fpx", s_state.atlases[2].w, s_state.atlases[2].h, s_state.atlases[2].px);

            s_state.isInitialized = true;
            return true;
        }

        /// @brief Reload system fonts (theme/content change).
        void Refresh() {
            if (!s_state.isInitialized)
                return;

            static const float big[] = { 64.0f, 56.0f, 48.0f, 40.0f };
            static const float sml[] = { 48.0f, 40.0f, 32.0f };
            static const AtlasSize textSizes[] = {
                { 512, 256 }, { 512, 384 }, { 512, 512 },
                { 1024, 256 }, { 1024, 384 }, { 1024, 512 }, { 1024, 768 }, { 1024, 1024 },
            };
            static const AtlasSize iconSizes[] = {
                { 512, 512 }, { 1024, 256 }, { 1024, 384 }, { 1024, 512 },
                { 1024, 768 }, { 1024, 1024 },
            };

            if (s_state.atlases[0].ok)
                BakeAndUpload(&s_state.atlases[0], s_state.latinData, big, 4,
                              textSizes, (int)(sizeof(textSizes) / sizeof(textSizes[0])));
            if (s_state.atlases[1].ok)
                BakeAndUpload(&s_state.atlases[1], s_state.latinData, big, 4,
                              textSizes, (int)(sizeof(textSizes) / sizeof(textSizes[0])));
            if (s_state.atlases[2].ok)
                BakeAndUpload(&s_state.atlases[2], s_state.iconData, sml, 3,
                              iconSizes, (int)(sizeof(iconSizes) / sizeof(iconSizes[0])));
        }

        /// @brief Whether font rendering is usable.
        /// @return True when initialized.
        bool Ok() {
            return s_state.isInitialized;
        }

        /// @brief Decode the next codepoint and advance the cursor.
        /// @param pp Pointer to the UTF-8 cursor (updated).
        /// @return Codepoint, or 0 at end of string.
        unsigned Next(const char **pp)
    {
        const uint8_t *p = (const uint8_t *)*pp;
        uint8_t c = *p;
        if (c < 0x80) {
            *pp = (const char *)(p + 1);
            return c;
        }
        unsigned cp;
        int need;
        if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; need = 1; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; need = 2; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; need = 3; }
        else { *pp = (const char *)(p + 1); return 0xFFFD; }
        for (int i = 0; i < need; i++) {
            uint8_t d = p[1 + i];
            if ((d & 0xC0) != 0x80) {
                *pp = (const char *)(p + 1);
                return 0xFFFD;
            }
            cp = (cp << 6) | (d & 0x3F);
        }
        *pp = (const char *)(p + 1 + need);
        return cp;
    }

        /// @brief Pick the atlas covering a codepoint.
        /// @param cp Unicode codepoint.
        /// @return Atlas pointer, or null when unavailable.
        static Atlas *Pick(unsigned cp)
    {
        if (cp >= 0xE000u && cp < 0xE200u && s_state.atlases[2].ok)
            return &s_state.atlases[2];
        if (cp >= 0x3040u && cp < 0x3100u && s_state.atlases[1].ok)
            return &s_state.atlases[1];
        if (s_state.atlases[0].ok)
            return &s_state.atlases[0];
        return 0;
    }

        /// @brief Walk a string, optionally emitting text quads.
        /// @param s UTF-8 text.
        /// @param x Start X.
        /// @param y Baseline Y.
        /// @param px Pixel size.
        /// @param dry True to measure without drawing.
        /// @param r Red 0-1.
        /// @param g Green 0-1.
        /// @param b Blue 0-1.
        /// @param a Alpha 0-1.
        /// @return End pen position.
        static float Pen(const char *s, float x, float y, float px, bool dry,
                        float r, float g, float b, float alpha)
    {
        if (!s || !s_state.isInitialized)
            return x;
        float pen = x;
        while (*s) {
            unsigned cp = Next(&s);
            Atlas *a = Pick(cp);
            if (!a)
                return pen;
            if (a != &s_state.atlases[0] &&
                (cp < (unsigned)a->first ||
                cp >= (unsigned)(a->first + a->count))) {
                a = &s_state.atlases[0];
                cp = 63u;
            }

            if (a == &s_state.atlases[0] && (cp < 32u || cp > 255u))
                cp = 63u;
            if (cp < (unsigned)a->first ||
                cp >= (unsigned)(a->first + a->count))
                continue;
            float scale = px / a->px;
            if (pen > 1920.0f - 16.0f)
                break;
            float before = pen;
            float xpos = pen;
            float ypos = y;
            int buf = (int)(a - s_state.atlases); /* text buffer, not texture slot */
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(a->g, a->w, a->h,
                            (int)(cp - (unsigned)a->first), &xpos, &ypos,
                            &q, 1);
            float x0 = before + (q.x0 - before) * scale;
            float x1 = before + (q.x1 - before) * scale;
            float y0 = y + (q.y0 - y) * scale;
            float y1 = y + (q.y1 - y) * scale;
            pen = before + (xpos - before) * scale;
            if (!dry && x1 > x0 && y1 > y0)
                Gfx::PushText(buf, x0, y0, x1, y1, q.s0, q.t0, q.s1, q.t1,
                            r, g, b, alpha);
        }
        return pen;
    }

        /// @brief Draw a string at a position.
        /// @param s UTF-8 text.
        /// @param x X position.
        /// @param y Baseline Y position.
        /// @param px Pixel size.
        /// @param r Red 0-1.
        /// @param g Green 0-1.
        /// @param b Blue 0-1.
        /// @param a Alpha 0-1.
        /// @return Advance width drawn.
        float Draw(const char *s, float x, float y, float px,
                float r, float g, float b, float a)
    {
        return Pen(s, x, y, px, false, r, g, b, a);
    }

        /// @brief Measure a string without drawing.
        /// @param s UTF-8 text.
        /// @param px Pixel size.
        /// @return Advance width.
        float Measure(const char *s, float px)
    {
        return Pen(s, 0.0f, 0.0f, px, true, 0.0f, 0.0f, 0.0f, 1.0f);
    }

        /// @brief Draw a string centered on X with a max width.
        /// @param s UTF-8 text.
        /// @param cx Center X.
        /// @param y Baseline Y position.
        /// @param px Pixel size.
        /// @param maxW Maximum width before squeezing.
        /// @param r Red 0-1.
        /// @param g Green 0-1.
        /// @param b Blue 0-1.
        /// @param a Alpha 0-1.
        void Centered(const char *s, float cx, float y, float px, float maxW,
                    float r, float g, float b, float a)
    {
        float w = Measure(s, px);
        while (w > maxW && px > 14.0f) {
            px -= 2.0f;
            w = Measure(s, px);
        }
        Draw(s, cx - w * 0.5f, y, px, r, g, b, a);
    }
} /* namespace Font */
