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

extern const uint8_t font_otf[];
extern const uint32_t font_otf_size;
extern const uint8_t switch_icons_ttf[];
extern const uint32_t switch_icons_ttf_size;

namespace Font {

    struct Atlas {
        int first;
        int count;
        int slot; /* Allocated by engine */
        stbtt_bakedchar g[512];
        float px;
        bool ok;
    };

    /// @brief All mutable font atlas state, single global.
    struct FontState {
        uint8_t bitmap[kFontAtlasWidth * kFontAtlasHeight];
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

        /// @brief Load a pack font file, else the embedded fallback.
        /// @param packDir Theme pack directory (may be null).
        /// @param fileName Pack-relative file name.
        /// @param fallbackData Embedded bytes.
        /// @param fallbackSize Embedded byte size.
        /// @param ownedOut Output owned buffer (free with free()).
        /// @param sizeOut Output byte size.
        /// @return Bytes to bake (owned or embedded).
        static const uint8_t *ResolveFontFile(const char *packDir, const char *fileName,
                                              const uint8_t *fallbackData, unsigned fallbackSize,
                                              uint8_t **ownedOut, unsigned *sizeOut)
        {
            *ownedOut = nullptr;
            *sizeOut = fallbackSize;

            if (packDir && packDir[0] && fileName && fileName[0]) {
                char fontPath[256];
                snprintf(fontPath, sizeof(fontPath), "%s/%s", packDir, fileName);
                fontPath[sizeof(fontPath) - 1] = 0;

                unsigned packSize = 0;
                unsigned char *packData = Packs::ReadFile(fontPath, kFontFileMax, &packSize);

                if (packData && packSize > 0) {
                    *ownedOut = packData;
                    *sizeOut = packSize;
                    Log::Line("[menu] pack font '%s'", fileName);
                    return packData;
                }

                free(packData);
            }

            return fallbackData;
        }

        /// @brief Bake one glyph range into the scratch atlas.
        /// @param atlas Atlas receiving the bake.
        /// @param file Font file bytes.
        /// @param heights Pixel heights to try.
        /// @param heightCount Height count.
        /// @return True when a height fit.
        static bool BakeOne(Atlas *atlas, const uint8_t *file, const float *heights, int heightCount)
        {
            for (int i = 0; i < heightCount; i++) {
                memset(s_state.bitmap, 0, sizeof(s_state.bitmap));

                int rows = stbtt_BakeFontBitmap(file, 0, heights[i], s_state.bitmap, kFontAtlasWidth, kFontAtlasHeight, atlas->first, atlas->count, atlas->g);
                Log::Line("[menu] Baked font atlas with %d rows", rows);

                if (rows > 0 && rows <= kFontAtlasHeight) {
                    atlas->px = heights[i];
                    return true;
                }
            }
            atlas->px = 0.0f;

            return false;
        }

        /// @brief Upload a baked atlas into GPU memory.
        /// @param a Atlas to upload.
        /// @return True on success.
        static bool UploadOne(Atlas *atlas) {
            if (!Gfx::TexUpload(atlas->slot, s_state.bitmap, kFontAtlasWidth, kFontAtlasHeight, Gfx::TexFormat::R8))
                return false;
            atlas->ok = true;
            return true;
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

        /// @brief Initialize font atlases and text buffers.
        /// @return True on success.
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
            s_state.atlases[0].first = 32;
            s_state.atlases[0].count = 224;
            s_state.atlases[0].slot = Gfx::TexAlloc();
            s_state.atlases[0].ok = false;

            s_state.atlases[1].first = 0x3040;
            s_state.atlases[1].count = 192;
            s_state.atlases[1].slot = Gfx::TexAlloc();
            s_state.atlases[1].ok = false;

            s_state.atlases[2].first = 0xE000;
            s_state.atlases[2].count = 512;
            s_state.atlases[2].slot = Gfx::TexAlloc();
            s_state.atlases[2].ok = false;

            if (s_state.atlases[0].slot < 0 || s_state.atlases[1].slot < 0 || s_state.atlases[2].slot < 0)
                return false;

            s_state.latinData = ResolveFontFile(packDir, "font.otf", font_otf, font_otf_size,
                                                &s_state.latinOwned, &s_state.latinSize);
            s_state.iconData = ResolveFontFile(packDir, "switch_icons.ttf", switch_icons_ttf,
                                               switch_icons_ttf_size, &s_state.iconOwned, &s_state.iconSize);

            if (!BakeOne(&s_state.atlases[0], s_state.latinData, big, 4)) {
                free(s_state.latinOwned);
                s_state.latinOwned = nullptr;
                s_state.latinData = font_otf;
                s_state.latinSize = font_otf_size;

                if (!BakeOne(&s_state.atlases[0], s_state.latinData, big, 4)) {
                    return false;
                }
            }

            if (!UploadOne(&s_state.atlases[0]))
                return false;

            if (BakeOne(&s_state.atlases[1], s_state.latinData, big, 4))
                UploadOne(&s_state.atlases[1]);

            if (!BakeOne(&s_state.atlases[2], s_state.iconData, sml, 3)) {
                free(s_state.iconOwned);
                s_state.iconOwned = nullptr;
                s_state.iconData = switch_icons_ttf;
                s_state.iconSize = switch_icons_ttf_size;

                if (BakeOne(&s_state.atlases[2], s_state.iconData, sml, 3)) {
                    UploadOne(&s_state.atlases[2]);
                }
            } else {
                UploadOne(&s_state.atlases[2]);
            }

            s_state.isInitialized = true;
            return true;
        }

        /// @brief Re-bake an atlas in place and re-upload its descriptor.
        /// @param a Atlas to refresh.
        /// @param file Font file bytes.
        /// @param heights Pixel heights to try.
        /// @param nHeights Height count.
        /// @return True on success.
        static bool RebakeOne(Atlas *a, const uint8_t *file, const float *heights, int nHeights)
        {
            if (!BakeOne(a, file, heights, nHeights))
                return false;
            return Gfx::TexUpload(a->slot, s_state.bitmap, kFontAtlasWidth, kFontAtlasHeight,
                                Gfx::TexFormat::R8);
        }

        /// @brief Reload system fonts (theme/content change).
        void Refresh() {
            if (!s_state.isInitialized)
                return;

            static const float big[] = { 64.0f, 56.0f, 48.0f, 40.0f };
            static const float sml[] = { 48.0f, 40.0f, 32.0f };

            RebakeOne(&s_state.atlases[0], s_state.latinData, big, 4);
            RebakeOne(&s_state.atlases[1], s_state.latinData, big, 4);
            RebakeOne(&s_state.atlases[2], s_state.iconData, sml, 3);
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
            stbtt_GetBakedQuad(a->g, kFontAtlasWidth, kFontAtlasHeight,
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

