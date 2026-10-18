/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering...         */

#include "Gfx.hpp"
#include "Log.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

namespace Gfx {

    namespace {

        constexpr int kGfxTextureSlotMax = kGfxMaxTextures;
        constexpr int kGfxCornerSegments = 6;
        constexpr int kGfxRingEdgeSegments = 2;
        constexpr int kGfxRingCornerSegments = 6;
        constexpr int kGfxRingMaxPoints = 64;
        constexpr int kGfxTempVertices = 512;

        /// @brief Display device state, part of the single graphics state.
        struct GfxDevice {
            SDL_Window *window;
            SDL_Renderer *renderer;
            bool isLive;
            bool isInitialized;
            int framebufferWidth;
            int framebufferHeight;
            float displayScale;
            float offsetX;
            float offsetY;
            float ringRotation;
        };

        /// @brief Texture slot store, part of the single graphics state.
        struct GfxTextureStore {
            SDL_Texture *handles[kGfxTextureSlotMax];
            int widths[kGfxTextureSlotMax];
            int heights[kGfxTextureSlotMax];
            bool streaming[kGfxTextureSlotMax];
        };

        static GfxDevice s_device = {};
        static GfxTextureStore s_textureStore = {};

        struct CQ {
            float x, y, r, g, b, a;
        };
        struct TQ {
            float x, y, u, v, r, g, b, a;
        };
        struct PQ {
            float x, y, w, h, rad, r, g, b, a;
        };
        struct IQ {
            float x, y, w, h, rTL, rTR, rBR, rBL, u0, v0, u1, v1, r, g, b, a;
            int slot;
        };
        struct HQ {
            float x, y, w, h, rad, thick, rot;
            float c0[4], c1[4], c2[4], c3[4];
        };

        /// @brief Queued draw commands, part of the single graphics state.
        struct GfxQueues {
            CQ colorQueue[kGfxVtxMax];
            int colorCount;
            TQ textQueue[3][kGfxTextMax];
            int textCount[3];
            PQ panelQueue[kGfxPanelMax];
            int panelCount;
            IQ iconQueue[kGfxIconQueueMax];
            int iconCount;
            HQ highlightQueue[kGfxHighlightMax];
            int highlightCount;
            CQ overlayColorQueue[kGfxOverlayColorMax];
            int overlayColorCount;
            PQ overlayPanelQueue[kGfxOverlayPanelMax];
            int overlayPanelCount;
            HQ overlayHighlightQueue[kGfxOverlayHighlightMax];
            int overlayHighlightCount;
        };

        /// @brief Batching state, part of the single graphics state.
        struct GfxBatch {
            SDL_Vertex vertices[4096];
            int vertexCount;
            SDL_Texture *texture;
            SDL_Vertex scratch[kGfxTempVertices];
        };

        static GfxQueues s_queues = {};
        static GfxBatch s_batch = {};

        static void Submit(SDL_Texture *textureSlot, SDL_Vertex *v, int n);

        /* Batching lives in s_batch. */

        static void FlushRun()
        {
            if (s_batch.vertexCount > 0) {
                Submit(s_batch.texture, s_batch.vertices, s_batch.vertexCount);
                s_batch.vertexCount = 0;
            }
        }

        static void EmitRun(SDL_Texture *texture, SDL_Vertex *vertices, int vertexCount)
        {
            int doneCount = 0;

            if (vertexCount <= 0) {
                return;
            }

            if (texture != s_batch.texture) {
                FlushRun();
            }

            s_batch.texture = texture;

            while (doneCount < vertexCount) {
                int roomCount = 4096 - s_batch.vertexCount;
                int takeCount = vertexCount - doneCount;

                if (takeCount > roomCount) {
                    takeCount = roomCount;
                }

                if (takeCount <= 0) {
                    FlushRun();
                    continue;
                }

                memcpy(&s_batch.vertices[s_batch.vertexCount], &vertices[doneCount], (size_t)takeCount * sizeof(SDL_Vertex));
                s_batch.vertexCount += takeCount;
                doneCount += takeCount;
            }
        }

        /// @brief Fit the 1920x1080 design into the framebuffer, letterboxed.
        static void FitTransform()
        {
            s_device.displayScale = (float)s_device.framebufferWidth / 1920.0f;
            float verticalScale = (float)s_device.framebufferHeight / 1080.0f;

            if (verticalScale < s_device.displayScale) {
                s_device.displayScale = verticalScale;
            }

            s_device.offsetX = ((float)s_device.framebufferWidth - 1920.0f * s_device.displayScale) * 0.5f;
            s_device.offsetY = ((float)s_device.framebufferHeight - 1080.0f * s_device.displayScale) * 0.5f;
        }

        /// @brief Push one triangle of flat color into a vertex buffer.
        /// @return False when the buffer is full.
        static bool EmitTri(SDL_Vertex *out, int *n, int cap,
                            float x0, float y0, float x1, float y1, float x2, float y2,
                            float r, float g, float b, float a) {
            if (*n + 3 > cap)
                return false;
        
            out[*n].position.x = x0;
            out[*n].position.y = y0;
            out[*n].tex_coord.x = 0.0f;
            out[*n].tex_coord.y = 0.0f;
            out[*n].color.r = r;
            out[*n].color.g = g;
            out[*n].color.b = b;
            out[*n].color.a = a;
            (*n)++;
        
            out[*n].position.x = x1;
            out[*n].position.y = y1;
            out[*n].tex_coord.x = 0.0f;
            out[*n].tex_coord.y = 0.0f;
            out[*n].color.r = r;
            out[*n].color.g = g;
            out[*n].color.b = b;
            out[*n].color.a = a;
            (*n)++;

            out[*n].position.x = x2;
            out[*n].position.y = y2;
            out[*n].tex_coord.x = 0.0f;
            out[*n].tex_coord.y = 0.0f;
            out[*n].color.r = r;
            out[*n].color.g = g;
            out[*n].color.b = b;
            out[*n].color.a = a;
            (*n)++;

            return true;
        }

        /// @brief Push one textured triangle into a vertex buffer.
        /// @return False when the buffer is full.
        static bool EmitTexTri(SDL_Vertex *out, int *n, int cap,
                               float x0, float y0, float u0, float v0,
                               float x1, float y1, float u1, float v1,
                               float x2, float y2, float u2, float v2,
                               float r, float g, float b, float a)
    {
        if (*n + 3 > cap)
            return false;
        out[*n].position.x = x0;
        out[*n].position.y = y0;
        out[*n].tex_coord.x = u0;
        out[*n].tex_coord.y = v0;
        out[*n].color.r = r;
        out[*n].color.g = g;
        out[*n].color.b = b;
        out[*n].color.a = a;
        (*n)++;
        out[*n].position.x = x1;
        out[*n].position.y = y1;
        out[*n].tex_coord.x = u1;
        out[*n].tex_coord.y = v1;
        out[*n].color.r = r;
        out[*n].color.g = g;
        out[*n].color.b = b;
        out[*n].color.a = a;
        (*n)++;
        out[*n].position.x = x2;
        out[*n].position.y = y2;
        out[*n].tex_coord.x = u2;
        out[*n].tex_coord.y = v2;
        out[*n].color.r = r;
        out[*n].color.g = g;
        out[*n].color.b = b;
        out[*n].color.a = a;
        (*n)++;
        return true;
    }

        /// @brief Tessellate a (possibly rounded) rect into flat triangles.
        /// @return Emitted vertex count.
        static int TessRRect(SDL_Vertex *out, int cap,
                             float x, float y, float w, float h, float cornerRadius,
                             float r, float g, float b, float a)
    {
        int n = 0;
        float rr = cornerRadius;
        float cx0, cy0, cx1, cy1;
        if (w <= 0.0f || h <= 0.0f)
            return 0;
        if (rr <= 0.0f) {
            EmitTri(out, &n, cap, x, y, x, y + h, x + w, y, r, g, b, a);
            EmitTri(out, &n, cap, x + w, y, x, y + h, x + w, y + h, r, g, b, a);
            return n;
        }
        if (rr > w * 0.5f)
            rr = w * 0.5f;
        if (rr > h * 0.5f)
            rr = h * 0.5f;
        cx0 = x + rr;
        cx1 = x + w - rr;
        cy0 = y + rr;
        cy1 = y + h - rr;
        EmitTri(out, &n, cap, cx0, cy0, cx0, cy1, cx1, cy0, r, g, b, a);
        EmitTri(out, &n, cap, cx1, cy0, cx0, cy1, cx1, cy1, r, g, b, a);
        EmitTri(out, &n, cap, x, cy0, x, cy1, cx0, cy0, r, g, b, a);
        EmitTri(out, &n, cap, cx0, cy0, x, cy1, cx0, cy1, r, g, b, a);
        EmitTri(out, &n, cap, cx1, cy0, cx1, cy1, x + w, cy0, r, g, b, a);
        EmitTri(out, &n, cap, x + w, cy0, cx1, cy1, x + w, cy1, r, g, b, a);
        EmitTri(out, &n, cap, cx0, y, cx0, cy0, cx1, y, r, g, b, a);
        EmitTri(out, &n, cap, cx1, y, cx0, cy0, cx1, cy0, r, g, b, a);
        EmitTri(out, &n, cap, cx0, cy1, cx0, y + h, cx1, cy1, r, g, b, a);
        EmitTri(out, &n, cap, cx1, cy1, cx0, y + h, cx1, y + h, r, g, b, a);
        {
            const float px[4] = { cx1, cx1, cx0, cx0 };
            const float py[4] = { cy0, cy1, cy1, cy0 };
            const float a0[4] = { -1.5707963f, 0.0f, 1.5707963f, 3.14159265f };
            for (int c = 0; c < 4; c++) {
                float px0 = px[c];
                float py0 = py[c];
                for (int s = 0; s < kGfxCornerSegments; s++) {
                    float t0 = a0[c] + 1.5707963f * (float)s / (float)kGfxCornerSegments;
                    float t1 = a0[c] + 1.5707963f * (float)(s + 1) / (float)kGfxCornerSegments;
                    float c0 = cosf(t0), s0 = sinf(t0);
                    float c1 = cosf(t1), s1 = sinf(t1);
                    /* Pixel coords (y down): P = center + R*(cos, sin). */
                    EmitTri(out, &n, cap, px0, py0,
                            px0 + c0 * rr, py0 + s0 * rr,
                            px0 + c1 * rr, py0 + s1 * rr, r, g, b, a);
                    if (n + 3 > cap)
                        return n;
                }
            }
        }
        return n;
    }

        /// @brief Map a quad position to texture UVs by bounds.
        static void QuadUV(float lx, float ly, float x, float y, float w, float h,
                           float u0, float v0, float u1, float v1,
                           float *uu, float *vv)
    {
        *uu = u0 + (lx - x) / w * (u1 - u0);
        *vv = v0 + (ly - y) / h * (v1 - v0);
    }

        /// @brief Emit one bounds-mapped textured triangle.
        static bool EmitQuadTri(SDL_Vertex *out, int *n, int cap,
                                float x0, float y0, float x1, float y1,
                                float x2, float y2,
                                float qx, float qy, float qw, float qh,
                                float u0, float v0, float u1, float v1,
                                float r, float g, float b, float a)
    {
        float uu[3], vv[3];
        QuadUV(x0, y0, qx, qy, qw, qh, u0, v0, u1, v1, &uu[0], &vv[0]);
        QuadUV(x1, y1, qx, qy, qw, qh, u0, v0, u1, v1, &uu[1], &vv[1]);
        QuadUV(x2, y2, qx, qy, qw, qh, u0, v0, u1, v1, &uu[2], &vv[2]);
        return EmitTexTri(out, n, cap, x0, y0, uu[0], vv[0],
                          x1, y1, uu[1], vv[1], x2, y2, uu[2], vv[2],
                          r, g, b, a);
    }

        /// @brief Tessellate a textured rounded rect (UVs follow bounds).
        /// Four independent corner radii (selected tiles: round top, square
        /// bottom). Triangulated as a fan from the rect center over the walked
        /// perimeter: a rounded rect with clamped radii is convex, so the fan
        /// covers it exactly with no overlap bookkeeping. Zero-radius corners
        /// yield harmless degenerate triangles.
        /// @return Emitted vertex count.
        static int TessRRectTex(SDL_Vertex *out, int cap,
                                float x, float y, float w, float h,
                                float cornerTopLeft, float cornerTopRight, float cornerBottomRight, float cornerBottomLeft,
                                float u0, float v0, float u1, float v1,
                                float r, float g, float b, float a)
    {
        int n = 0;
        float cx, cy;
        struct PP { float x, y; };
        PP pts[64];
        int np = 0;
        if (w <= 0.0f || h <= 0.0f)
            return 0;
        if (cornerTopLeft < 0.0f)
            cornerTopLeft = 0.0f;
        if (cornerTopRight < 0.0f)
            cornerTopRight = 0.0f;
        if (cornerBottomRight < 0.0f)
            cornerBottomRight = 0.0f;
        if (cornerBottomLeft < 0.0f)
            cornerBottomLeft = 0.0f;
        if (cornerTopLeft > w * 0.5f)
            cornerTopLeft = w * 0.5f;
        if (cornerTopRight > w * 0.5f)
            cornerTopRight = w * 0.5f;
        if (cornerBottomRight > w * 0.5f)
            cornerBottomRight = w * 0.5f;
        if (cornerBottomLeft > w * 0.5f)
            cornerBottomLeft = w * 0.5f;
        if (cornerTopLeft + cornerBottomLeft > h) {
            float k = h / (cornerTopLeft + cornerBottomLeft);
            cornerTopLeft *= k;
            cornerBottomLeft *= k;
        }
        if (cornerTopRight + cornerBottomRight > h) {
            float k = h / (cornerTopRight + cornerBottomRight);
            cornerTopRight *= k;
            cornerBottomRight *= k;
        }
        if (cornerTopLeft <= 0.0f && cornerTopRight <= 0.0f && cornerBottomRight <= 0.0f && cornerBottomLeft <= 0.0f) {
            EmitQuadTri(out, &n, cap, x, y, x, y + h, x + w, y,
                        x, y, w, h, u0, v0, u1, v1, r, g, b, a);
            EmitQuadTri(out, &n, cap, x + w, y, x, y + h, x + w, y + h,
                        x, y, w, h, u0, v0, u1, v1, r, g, b, a);
            return n;
        }
        cx = x + w * 0.5f;
        cy = y + h * 0.5f;
        {
            /* Clockwise perimeter in y-down pixels, starting at top-left
               edge. Angles: -90 deg is up. */
            auto push = [&](float px, float py) {
                if (np < 64) {
                    pts[np].x = px;
                    pts[np].y = py;
                    np++;
                }
            };
            auto arc = [&](float ccx, float ccy, float rr, float a0) {
                for (int s = 1; s <= kGfxCornerSegments; s++) {
                    float t = a0 + 1.5707963f * (float)s / (float)kGfxCornerSegments;
                    push(ccx + cosf(t) * rr, ccy + sinf(t) * rr);
                }
            };
            push(x + cornerTopLeft, y);
            push(x + w - cornerTopRight, y);
            arc(x + w - cornerTopRight, y + cornerTopRight, cornerTopRight, -1.5707963f);
            push(x + w, y + h - cornerBottomRight);
            arc(x + w - cornerBottomRight, y + h - cornerBottomRight, cornerBottomRight, 0.0f);
            push(x + cornerBottomLeft, y + h);
            arc(x + cornerBottomLeft, y + h - cornerBottomLeft, cornerBottomLeft, 1.5707963f);
            push(x, y + cornerTopLeft);
            arc(x + cornerTopLeft, y + cornerTopLeft, cornerTopLeft, 3.14159265f);
        }
        for (int i = 0; i < np; i++) {
            int j = (i + 1) % np;
            if (!EmitQuadTri(out, &n, cap, cx, cy,
                             pts[i].x, pts[i].y, pts[j].x, pts[j].y,
                             x, y, w, h, u0, v0, u1, v1, r, g, b, a))
                return n;
        }
        return n;
    }

        /// @brief Gradient color at ring arclength fraction (mirrors hl_fsh).
        static void RingColor(float t, float rot, const float *c0, const float *c1,
                              const float *c2, const float *c3, float *r, float *g, float *b)
    {
        float sn = t + rot - (float)(int)(t + rot);
        if (sn < 0.0f)
            sn += 1.0f;
        float seg = sn * 4.0f;
        int i = ((int)seg) & 3;
        float f = seg - (float)(int)seg;
        const float *A = (i == 0) ? c3 : (i == 1) ? c0 : (i == 2) ? c1 : c2;
        const float *B = (i == 0) ? c0 : (i == 1) ? c1 : (i == 2) ? c2 : c3;
        *r = A[0] + (B[0] - A[0]) * f;
        *g = A[1] + (B[1] - A[1]) * f;
        *b = A[2] + (B[2] - A[2]) * f;
    }

        /// @brief Tessellate a gradient highlight ring into a triangle strip mesh.
        /// @return Emitted vertex count.
        static int TessRing(SDL_Vertex *out, int cap,
                            float x, float y, float w, float h, float cornerRadius, float thick,
                            const float *c0, const float *c1, const float *c2, const float *c3,
                            float rot)
    {
        float R = cornerRadius, T = thick;
        float mn = w < h ? w : h;
        float Wi, Hi, Ri;
        struct RP { float ox, oy, ix, iy; };
        RP pts[kGfxRingMaxPoints];
        int npt = 0;
        float total = 0.0f;
        float acc[kGfxRingMaxPoints];
        int n = 0;
        if (w <= 0.0f || h <= 0.0f)
            return 0;
        if (R > mn * 0.5f)
            R = mn * 0.5f;
        if (T < 1.0f)
            T = 1.0f;
        if (T > mn * 0.5f - 1.0f)
            T = mn * 0.5f - 1.0f;
        Wi = w - 2.0f * T;
        Hi = h - 2.0f * T;
        if (Wi < 1.0f)
            Wi = 1.0f;
        if (Hi < 1.0f)
            Hi = 1.0f;
        Ri = R - T;
        if (Ri < 0.0f)
            Ri = 0.0f;
        {
            /* Clockwise from the top edge, mirroring hl_fsh arclength. */
            float ecx[4] = { x + w - R, x + w - R, x + R, x + R };
            float ecy[4] = { y + R, y + h - R, y + h - R, y + R };
            float icx[4] = { x + T + Wi - Ri, x + T + Wi - Ri, x + T + Ri, x + T + Ri };
            float icy[4] = { y + T + Ri, y + T + Hi - Ri, y + T + Hi - Ri, y + T + Ri };
            float ea[4] = { -1.5707963f, 0.0f, 1.5707963f, 3.14159265f };
            auto push = [&](float ox, float oy, float ix, float iy) {
                if (npt >= kGfxRingMaxPoints)
                    return;
                if (npt > 0) {
                    float dx = ox - pts[npt - 1].ox;
                    float dy = oy - pts[npt - 1].oy;
                    total += sqrtf(dx * dx + dy * dy);
                }
                acc[npt] = total;
                pts[npt].ox = ox;
                pts[npt].oy = oy;
                pts[npt].ix = ix;
                pts[npt].iy = iy;
                npt++;
            };
            push(x + R, y, x + T + Ri, y + T);
            for (int c = 0; c < 4; c++) {
                float sx0 = pts[npt - 1].ox, sy0 = pts[npt - 1].oy;
                float six0 = pts[npt - 1].ix, siy0 = pts[npt - 1].iy;
                float nx, ny, nix, niy;
                if (c == 0) {
                    nx = x + w - R;
                    ny = y;
                    nix = x + T + Wi - Ri;
                    niy = y + T;
                } else if (c == 1) {
                    nx = x + w;
                    ny = y + h - R;
                    nix = x + T + Wi;
                    niy = y + T + Hi - Ri;
                } else if (c == 2) {
                    nx = x + R;
                    ny = y + h;
                    nix = x + T + Ri;
                    niy = y + T + Hi;
                } else {
                    nx = x;
                    ny = y + R;
                    nix = x + T;
                    niy = y + T + Ri;
                }
                for (int s = 1; s <= kGfxRingEdgeSegments; s++) {
                    float t = (float)s / (float)kGfxRingEdgeSegments;
                    push(sx0 + (nx - sx0) * t, sy0 + (ny - sy0) * t,
                         six0 + (nix - six0) * t, siy0 + (niy - siy0) * t);
                }
                for (int s = 1; s <= kGfxRingCornerSegments; s++) {
                    float t = ea[c] + 1.5707963f * (float)s / (float)kGfxRingCornerSegments;
                    float co = cosf(t), si = sinf(t);
                    float ox = ecx[c] + co * R;
                    float oy = ecy[c] + si * R;
                    float ix = icx[c] + co * Ri;
                    float iy = icy[c] + si * Ri;
                    push(ox, oy, ix, iy);
                }
            }
        }
        if (npt < 2 || total <= 0.0f)
            return 0;
        for (int i = 0; i + 1 < npt; i++) {
            float t = acc[i] / total;
            float r, g, b;
            float t2 = acc[i + 1] / total;
            float r2, g2, b2;
            RingColor(t, rot, c0, c1, c2, c3, &r, &g, &b);
            RingColor(t2, rot, c0, c1, c2, c3, &r2, &g2, &b2);
            if (n + 6 > cap)
                return n;
            SDL_Vertex *v = &out[n];
            v[0].position.x = pts[i].ox;
            v[0].position.y = pts[i].oy;
            v[0].tex_coord.x = 0.0f;
            v[0].tex_coord.y = 0.0f;
            v[0].color.r = r;
            v[0].color.g = g;
            v[0].color.b = b;
            v[0].color.a = 1.0f;
            v[1].position.x = pts[i].ix;
            v[1].position.y = pts[i].iy;
            v[1].tex_coord.x = 0.0f;
            v[1].tex_coord.y = 0.0f;
            v[1].color.r = r;
            v[1].color.g = g;
            v[1].color.b = b;
            v[1].color.a = 1.0f;
            v[2].position.x = pts[i + 1].ox;
            v[2].position.y = pts[i + 1].oy;
            v[2].tex_coord.x = 0.0f;
            v[2].tex_coord.y = 0.0f;
            v[2].color.r = r2;
            v[2].color.g = g2;
            v[2].color.b = b2;
            v[2].color.a = 1.0f;
            v[3].position.x = pts[i + 1].ox;
            v[3].position.y = pts[i + 1].oy;
            v[3].tex_coord.x = 0.0f;
            v[3].tex_coord.y = 0.0f;
            v[3].color.r = r2;
            v[3].color.g = g2;
            v[3].color.b = b2;
            v[3].color.a = 1.0f;
            v[4].position.x = pts[i].ix;
            v[4].position.y = pts[i].iy;
            v[4].tex_coord.x = 0.0f;
            v[4].tex_coord.y = 0.0f;
            v[4].color.r = r;
            v[4].color.g = g;
            v[4].color.b = b;
            v[4].color.a = 1.0f;
            v[5].position.x = pts[i + 1].ix;
            v[5].position.y = pts[i + 1].iy;
            v[5].tex_coord.x = 0.0f;
            v[5].tex_coord.y = 0.0f;
            v[5].color.r = r2;
            v[5].color.g = g2;
            v[5].color.b = b2;
            v[5].color.a = 1.0f;
            n += 6;
        }
        return n;
    }

        /// @brief Submit flat triangles.
        static void Submit(SDL_Texture *textureSlot, SDL_Vertex *v, int n)
    {
        if (n <= 0 || !s_device.renderer)
            return;
        SDL_RenderGeometry(s_device.renderer, textureSlot, v, n, NULL, 0);
    }

    } /* anonymous namespace */

    bool Init(NWindow *win, int framebufferWidth, int framebufferHeight)
    {
        Destroy();
        s_device.framebufferWidth = framebufferWidth;
        s_device.framebufferHeight = framebufferHeight;
        FitTransform();

        SDL_SetMainReady();
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            Log::Line("[menu] SDL_Init failed");
            return false;
        }
        s_device.isInitialized = true;

        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_SWITCH_NWINDOW_POINTER, win);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, framebufferWidth);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, framebufferHeight);
        s_device.window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);
        if (!s_device.window) {
            Log::Line("[menu] window failed");
            Destroy();
            return false;
        }

        s_device.renderer = SDL_CreateRenderer(s_device.window, "deko3d");
        if (!s_device.renderer) {
            Log::Line("[menu] renderer failed");
            Destroy();
            return false;
        }
        SDL_SetRenderDrawBlendMode(s_device.renderer, SDL_BLENDMODE_BLEND);

        ResetCounts();
        s_device.isLive = true;
        return true;
    }

    void Destroy()
    {
        s_device.isLive = false;
        for (int i = 0; i < kGfxTextureSlotMax; i++) {
            if (s_textureStore.handles[i]) {
                SDL_DestroyTexture(s_textureStore.handles[i]);
                s_textureStore.handles[i] = NULL;
            }
            s_textureStore.widths[i] = 0;
            s_textureStore.heights[i] = 0;
            s_textureStore.streaming[i] = false;
        }
        if (s_device.renderer) {
            SDL_DestroyRenderer(s_device.renderer);
            s_device.renderer = NULL;
        }
        if (s_device.window) {
            SDL_DestroyWindow(s_device.window);
            s_device.window = NULL;
        }
        if (s_device.isInitialized) {
            SDL_Quit();
            s_device.isInitialized = false;
        }
    }

    bool Ready()
    {
        return s_device.isLive;
    }

    bool BeginFrame()
    {
        return s_device.isLive;
    }

    void DrawColor()
    {
        int n = 0;
        for (int i = 0; i + 6 <= s_queues.colorCount; i += 6) {
            if (n + 6 > kGfxTempVertices) {
                EmitRun(NULL, s_batch.scratch, n);
                n = 0;
            }
            for (int k = 0; k < 6; k++) {
                s_batch.scratch[n].position.x = s_queues.colorQueue[i + k].x;
                s_batch.scratch[n].position.y = s_queues.colorQueue[i + k].y;
                s_batch.scratch[n].tex_coord.x = 0.0f;
                s_batch.scratch[n].tex_coord.y = 0.0f;
                s_batch.scratch[n].color.r = s_queues.colorQueue[i + k].r;
                s_batch.scratch[n].color.g = s_queues.colorQueue[i + k].g;
                s_batch.scratch[n].color.b = s_queues.colorQueue[i + k].b;
                s_batch.scratch[n].color.a = s_queues.colorQueue[i + k].a;
                n++;
            }
        }
        EmitRun(NULL, s_batch.scratch, n);
        FlushRun();
    }

    void DrawPanelsBg()
    {
        int n = 0;
        if (s_queues.panelCount <= 0)
            return;
        n = TessRRect(s_batch.scratch, kGfxTempVertices, s_queues.panelQueue[0].x, s_queues.panelQueue[0].y,
                      s_queues.panelQueue[0].w, s_queues.panelQueue[0].h, s_queues.panelQueue[0].rad,
                      s_queues.panelQueue[0].r, s_queues.panelQueue[0].g, s_queues.panelQueue[0].b, s_queues.panelQueue[0].a);
        EmitRun(NULL, s_batch.scratch, n);
        FlushRun();
    }

    void DrawPanels()
    {
        for (int i = 1; i < s_queues.panelCount; i++) {
            int n = TessRRect(s_batch.scratch, kGfxTempVertices, s_queues.panelQueue[i].x, s_queues.panelQueue[i].y,
                              s_queues.panelQueue[i].w, s_queues.panelQueue[i].h, s_queues.panelQueue[i].rad,
                              s_queues.panelQueue[i].r, s_queues.panelQueue[i].g, s_queues.panelQueue[i].b, s_queues.panelQueue[i].a);
            EmitRun(NULL, s_batch.scratch, n);
        }
        FlushRun();
    }

    void DrawHighlights()
    {
        for (int i = 0; i < s_queues.highlightCount; i++) {
            HQ *q = &s_queues.highlightQueue[i];
            int n = TessRing(s_batch.scratch, kGfxTempVertices, q->x, q->y, q->w, q->h, q->rad,
                             q->thick, q->c0, q->c1, q->c2, q->c3, q->rot);
            EmitRun(NULL, s_batch.scratch, n);
        }
        FlushRun();
    }

    void DrawIcons()
    {
        for (int i = 0; i < s_queues.iconCount; i++) {
            IQ *q = &s_queues.iconQueue[i];
            int n;
            if (!TexValid(q->slot) || q->slot >= kGfxTextureSlotMax || !s_textureStore.handles[q->slot])
                continue;
            n = TessRRectTex(s_batch.scratch, kGfxTempVertices, q->x, q->y, q->w, q->h,
                             q->rTL, q->rTR, q->rBR, q->rBL,
                             q->u0, q->v0, q->u1, q->v1, q->r, q->g, q->b, q->a);
            EmitRun(s_textureStore.handles[q->slot], s_batch.scratch, n);
        }
        FlushRun();
    }

    void DrawOverlay()
    {
        int n = 0;
        for (int i = 0; i + 6 <= s_queues.overlayColorCount; i += 6) {
            if (n + 6 > kGfxTempVertices) {
                EmitRun(NULL, s_batch.scratch, n);
                n = 0;
            }
            for (int k = 0; k < 6; k++) {
                s_batch.scratch[n].position.x = s_queues.overlayColorQueue[i + k].x;
                s_batch.scratch[n].position.y = s_queues.overlayColorQueue[i + k].y;
                s_batch.scratch[n].tex_coord.x = 0.0f;
                s_batch.scratch[n].tex_coord.y = 0.0f;
                s_batch.scratch[n].color.r = s_queues.overlayColorQueue[i + k].r;
                s_batch.scratch[n].color.g = s_queues.overlayColorQueue[i + k].g;
                s_batch.scratch[n].color.b = s_queues.overlayColorQueue[i + k].b;
                s_batch.scratch[n].color.a = s_queues.overlayColorQueue[i + k].a;
                n++;
            }
        }
        EmitRun(NULL, s_batch.scratch, n);

        for (int i = 0; i < s_queues.overlayPanelCount; i++) {
            n = TessRRect(s_batch.scratch, kGfxTempVertices, s_queues.overlayPanelQueue[i].x, s_queues.overlayPanelQueue[i].y,
                          s_queues.overlayPanelQueue[i].w, s_queues.overlayPanelQueue[i].h, s_queues.overlayPanelQueue[i].rad,
                          s_queues.overlayPanelQueue[i].r, s_queues.overlayPanelQueue[i].g, s_queues.overlayPanelQueue[i].b,
                          s_queues.overlayPanelQueue[i].a);
            EmitRun(NULL, s_batch.scratch, n);
        }

        for (int i = 0; i < s_queues.overlayHighlightCount; i++) {
            HQ *q = &s_queues.overlayHighlightQueue[i];
            n = TessRing(s_batch.scratch, kGfxTempVertices, q->x, q->y, q->w, q->h, q->rad,
                         q->thick, q->c0, q->c1, q->c2, q->c3, q->rot);
            EmitRun(NULL, s_batch.scratch, n);
        }

        FlushRun();
    }

    void DrawTextBuf(int texSlot, int buf)
    {
        int n = 0;
        if (texSlot < 0 || texSlot >= kGfxTextureSlotMax || !s_textureStore.handles[texSlot])
            return;
        if (buf < 0 || buf > 2)
            return;
        for (int i = 0; i + 6 <= s_queues.textCount[buf]; i += 6) {
            if (n + 6 > kGfxTempVertices) {
                EmitRun(s_textureStore.handles[texSlot], s_batch.scratch, n);
                n = 0;
            }
            for (int k = 0; k < 6; k++) {
                s_batch.scratch[n].position.x = s_queues.textQueue[buf][i + k].x;
                s_batch.scratch[n].position.y = s_queues.textQueue[buf][i + k].y;
                s_batch.scratch[n].tex_coord.x = s_queues.textQueue[buf][i + k].u;
                s_batch.scratch[n].tex_coord.y = s_queues.textQueue[buf][i + k].v;
                s_batch.scratch[n].color.r = s_queues.textQueue[buf][i + k].r;
                s_batch.scratch[n].color.g = s_queues.textQueue[buf][i + k].g;
                s_batch.scratch[n].color.b = s_queues.textQueue[buf][i + k].b;
                s_batch.scratch[n].color.a = s_queues.textQueue[buf][i + k].a;
                n++;
            }
        }
        EmitRun(s_textureStore.handles[texSlot], s_batch.scratch, n);
        FlushRun();
    }

    void EndFrame()
    {
        if (s_device.isLive)
            SDL_RenderPresent(s_device.renderer);
    }

    float X(float x)
    {
        return s_device.offsetX + x * s_device.displayScale;
    }

    float Y(float y)
    {
        return s_device.offsetY + y * s_device.displayScale;
    }

    float SC(float v)
    {
        return v * s_device.displayScale;
    }

    int TexAlloc()
    {
        for (int i = 0; i < kGfxTextureSlotMax; i++) {
            if (!s_textureStore.handles[i] && s_textureStore.widths[i] == 0) {
                s_textureStore.widths[i] = -1;
                return i;
            }
        }
        return -1;
    }

    bool TexValid(int slot)
    {
        return slot >= 0;
    }

    void TexFree(int slot)
    {
        if (slot < 0 || slot >= kGfxTextureSlotMax)
            return;
        if (!s_textureStore.handles[slot] && s_textureStore.widths[slot] == 0)
            return; // never allocated: stale handle, nothing to do
        if (s_textureStore.handles[slot]) {
            SDL_DestroyTexture(s_textureStore.handles[slot]);
            s_textureStore.handles[slot] = NULL;
        }
        s_textureStore.widths[slot] = 0;
        s_textureStore.heights[slot] = 0;
        s_textureStore.streaming[slot] = false;
    }

    bool TexUpload(int slot, const uint8_t *px, int w, int h, TexFormat fmt)
    {
        SDL_Rect rect;
        if (slot < 0 || slot >= kGfxTextureSlotMax || !px || w <= 0 || h <= 0 || !s_device.isLive)
            return false;
        if (s_textureStore.handles[slot] && (s_textureStore.widths[slot] != w || s_textureStore.heights[slot] != h || s_textureStore.streaming[slot])) {
            SDL_DestroyTexture(s_textureStore.handles[slot]);
            s_textureStore.handles[slot] = NULL;
        }
        if (!s_textureStore.handles[slot]) {
            s_textureStore.handles[slot] = SDL_CreateTexture(s_device.renderer, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_STATIC, w, h);
            if (!s_textureStore.handles[slot])
                return false;
            s_textureStore.widths[slot] = w;
            s_textureStore.heights[slot] = h;
            s_textureStore.streaming[slot] = false;
        }
        rect.x = 0;
        rect.y = 0;
        rect.w = w;
        rect.h = h;
        if (fmt == TexFormat::R8) {
            uint8_t *tmp = (uint8_t *)malloc((size_t)w * (size_t)h * 4);
            bool ok;
            if (!tmp)
                return false;
            for (int i = 0; i < w * h; i++) {
                tmp[i * 4 + 0] = 255;
                tmp[i * 4 + 1] = 255;
                tmp[i * 4 + 2] = 255;
                tmp[i * 4 + 3] = px[i];
            }
            ok = SDL_UpdateTexture(s_textureStore.handles[slot], &rect, tmp, w * 4);
            free(tmp);
            return ok;
        }
        return SDL_UpdateTexture(s_textureStore.handles[slot], &rect, px, w * 4);
    }

    bool TexLock(int slot, int w, int h, void **pixels, int *pitch)
    {
        if (slot < 0 || slot >= kGfxTextureSlotMax || !pixels || !pitch || !s_device.isLive)
            return false;
        if (s_textureStore.handles[slot] && (s_textureStore.widths[slot] != w || s_textureStore.heights[slot] != h || !s_textureStore.streaming[slot])) {
            SDL_DestroyTexture(s_textureStore.handles[slot]);
            s_textureStore.handles[slot] = NULL;
        }
        if (!s_textureStore.handles[slot]) {
            s_textureStore.handles[slot] = SDL_CreateTexture(s_device.renderer, SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_STREAMING, w, h);
            if (!s_textureStore.handles[slot])
                return false;
            s_textureStore.widths[slot] = w;
            s_textureStore.heights[slot] = h;
            s_textureStore.streaming[slot] = true;
        }
        return SDL_LockTexture(s_textureStore.handles[slot], NULL, pixels, pitch);
    }

    void TexUnlock(int slot)
    {
        if (slot < 0 || slot >= kGfxTextureSlotMax || !s_textureStore.handles[slot])
            return;
        SDL_UnlockTexture(s_textureStore.handles[slot]);
    }

    void PushQuad(float positionX, float positionY, float width, float height,
                  float red, float green, float blue, float alpha)
    {
        float frameX0 = X(positionX);
        float frameY0 = Y(positionY);
        float frameX1 = X(positionX + width);
        float frameY1 = Y(positionY + height);
        CQ *quadVertices;

        if (s_queues.colorCount + 6 > kGfxVtxMax) {
            return;
        }

        quadVertices = &s_queues.colorQueue[s_queues.colorCount];
        quadVertices[0].x = frameX0;
        quadVertices[0].y = frameY0;
        quadVertices[1].x = frameX0;
        quadVertices[1].y = frameY1;
        quadVertices[2].x = frameX1;
        quadVertices[2].y = frameY0;
        quadVertices[3].x = frameX1;
        quadVertices[3].y = frameY0;
        quadVertices[4].x = frameX0;
        quadVertices[4].y = frameY1;
        quadVertices[5].x = frameX1;
        quadVertices[5].y = frameY1;

        for (int vertexIndex = 0; vertexIndex < 6; vertexIndex++) {
            quadVertices[vertexIndex].r = red;
            quadVertices[vertexIndex].g = green;
            quadVertices[vertexIndex].b = blue;
            quadVertices[vertexIndex].a = alpha;
        }

        s_queues.colorCount += 6;
    }

    void PushPanel(float positionX, float positionY, float width, float height, float cornerRadius,
                   float red, float green, float blue, float alpha)
    {
        PQ *panelQuad;

        if (s_queues.panelCount + 1 > kGfxPanelMax) {
            return;
        }

        if (width <= 0.0f || height <= 0.0f) {
            return;
        }

        panelQuad = &s_queues.panelQueue[s_queues.panelCount];
        panelQuad->x = X(positionX);
        panelQuad->y = Y(positionY);
        panelQuad->w = SC(width);
        panelQuad->h = SC(height);
        panelQuad->rad = SC(cornerRadius);
        panelQuad->r = red;
        panelQuad->g = green;
        panelQuad->b = blue;
        panelQuad->a = alpha;
        s_queues.panelCount++;
    }

    unsigned PushIcon(float positionX, float positionY, float width, float height,
                      float cornerTopLeft, float cornerTopRight, float cornerBottomRight, float cornerBottomLeft,
                      float red, float green, float blue, float border, int textureSlot,
                      float leftU, float topV, float rightU, float bottomV, float edgeBlend, float alpha)
    {
        IQ *iconQuad;
        unsigned quadId;
        (void)border;
        (void)edgeBlend;

        if (s_queues.iconCount + 1 > kGfxIconQueueMax) {
            return 0xFFFFFFFFu;
        }

        if (width <= 0.0f || height <= 0.0f) {
            return 0xFFFFFFFFu;
        }

        quadId = (unsigned)s_queues.iconCount;
        iconQuad = &s_queues.iconQueue[s_queues.iconCount];
        iconQuad->x = X(positionX);
        iconQuad->y = Y(positionY);
        iconQuad->w = SC(width);
        iconQuad->h = SC(height);
        iconQuad->rTL = SC(cornerTopLeft);
        iconQuad->rTR = SC(cornerTopRight);
        iconQuad->rBR = SC(cornerBottomRight);
        iconQuad->rBL = SC(cornerBottomLeft);
        iconQuad->u0 = leftU;
        iconQuad->v0 = topV;
        iconQuad->u1 = rightU;
        iconQuad->v1 = bottomV;
        iconQuad->r = red;
        iconQuad->g = green;
        iconQuad->b = blue;
        iconQuad->a = alpha;
        iconQuad->slot = textureSlot;
        s_queues.iconCount++;
        return quadId;
    }

    void PushSelectRing(float positionX, float positionY, float width, float height, float cornerRadius, float ringThickness)
    {
        static const float ringColor0[4] = { 0.537f, 0.584f, 0.945f, 1.0f };
        static const float ringColor1[4] = { 0.384f, 0.588f, 0.949f, 1.0f };
        static const float ringColor2[4] = { 0.863f, 0.722f, 0.875f, 1.0f };
        static const float ringColor3[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        PushHighlight(positionX, positionY, width, height, cornerRadius, ringThickness, ringColor0, ringColor1, ringColor2, ringColor3, s_device.ringRotation);
    }

    void PushOverlayQuad(float x, float y, float w, float h,
                         float r, float g, float b, float a)
    {
        float X0 = X(x), Y0 = Y(y), X1 = X(x + w), Y1 = Y(y + h);
        CQ *v;
        if (s_queues.overlayColorCount + 6 > kGfxOverlayColorMax)
            return;
        v = &s_queues.overlayColorQueue[s_queues.overlayColorCount];
        v[0].x = X0;
        v[0].y = Y0;
        v[1].x = X0;
        v[1].y = Y1;
        v[2].x = X1;
        v[2].y = Y0;
        v[3].x = X1;
        v[3].y = Y0;
        v[4].x = X0;
        v[4].y = Y1;
        v[5].x = X1;
        v[5].y = Y1;
        for (int i = 0; i < 6; i++) {
            v[i].r = r;
            v[i].g = g;
            v[i].b = b;
            v[i].a = a;
        }
        s_queues.overlayColorCount += 6;
    }

    void PushOverlayPanel(float x, float y, float w, float h, float cornerRadius,
                          float r, float g, float b, float a)
    {
        PQ *q;
        if (s_queues.overlayPanelCount + 1 > kGfxOverlayPanelMax)
            return;
        if (w <= 0.0f || h <= 0.0f)
            return;
        q = &s_queues.overlayPanelQueue[s_queues.overlayPanelCount];
        q->x = X(x);
        q->y = Y(y);
        q->w = SC(w);
        q->h = SC(h);
        q->rad = SC(cornerRadius);
        q->r = r;
        q->g = g;
        q->b = b;
        q->a = a;
        s_queues.overlayPanelCount++;
    }

    void PushOverlayRing(float x, float y, float w, float h, float cornerRadius, float thick)
    {
        static const float c0[4] = { 0.537f, 0.584f, 0.945f, 1.0f };
        static const float c1[4] = { 0.384f, 0.588f, 0.949f, 1.0f };
        static const float c2[4] = { 0.863f, 0.722f, 0.875f, 1.0f };
        static const float c3[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        HQ *q;
        if (s_queues.overlayHighlightCount + 1 > kGfxOverlayHighlightMax)
            return;
        if (w <= 0.0f || h <= 0.0f || thick <= 0.0f)
            return;
        q = &s_queues.overlayHighlightQueue[s_queues.overlayHighlightCount];
        q->x = X(x);
        q->y = Y(y);
        q->w = SC(w);
        q->h = SC(h);
        q->rad = SC(cornerRadius);
        q->thick = SC(thick);
        q->rot = s_device.ringRotation;
        for (int i = 0; i < 4; i++) {
            q->c0[i] = c0[i];
            q->c1[i] = c1[i];
            q->c2[i] = c2[i];
            q->c3[i] = c3[i];
        }
        s_queues.overlayHighlightCount++;
    }

    void PushHighlight(float x, float y, float w, float h, float cornerRadius,
                       float thick, const float *c0, const float *c1,
                       const float *c2, const float *c3, float rot)
    {
        HQ *q;
        if (s_queues.highlightCount + 1 > kGfxHighlightMax)
            return;
        if (w <= 0.0f || h <= 0.0f || thick <= 0.0f)
            return;
        q = &s_queues.highlightQueue[s_queues.highlightCount];
        q->x = X(x);
        q->y = Y(y);
        q->w = SC(w);
        q->h = SC(h);
        q->rad = SC(cornerRadius);
        q->thick = SC(thick);
        q->rot = rot;
        for (int i = 0; i < 4; i++) {
            q->c0[i] = c0[i];
            q->c1[i] = c1[i];
            q->c2[i] = c2[i];
            q->c3[i] = c3[i];
        }
        s_queues.highlightCount++;
    }

    void PushText(int buf, float x0, float y0, float x1, float y1,
                  float s0, float t0, float s1, float t1,
                  float r, float g, float b, float a)
    {
        TQ *v;
        float X0 = X(x0), Y0 = Y(y0), X1 = X(x1), Y1 = Y(y1);
        if (buf < 0 || buf > 2 || s_queues.textCount[buf] + 6 > kGfxTextMax)
            return;
        if (x1 <= x0 || y1 <= y0)
            return;
        v = &s_queues.textQueue[buf][s_queues.textCount[buf]];
        v[0].x = X0;
        v[0].y = Y0;
        v[0].u = s0;
        v[0].v = t0;
        v[1].x = X0;
        v[1].y = Y1;
        v[1].u = s0;
        v[1].v = t1;
        v[2].x = X1;
        v[2].y = Y0;
        v[2].u = s1;
        v[2].v = t0;
        v[3].x = X1;
        v[3].y = Y0;
        v[3].u = s1;
        v[3].v = t0;
        v[4].x = X0;
        v[4].y = Y1;
        v[4].u = s0;
        v[4].v = t1;
        v[5].x = X1;
        v[5].y = Y1;
        v[5].u = s1;
        v[5].v = t1;
        for (int i = 0; i < 6; i++) {
            v[i].r = r;
            v[i].g = g;
            v[i].b = b;
            v[i].a = a;
        }
        s_queues.textCount[buf] += 6;
    }

    void ResetCounts()
    {
        s_device.ringRotation += 0.5f / 60.0f;
        if (s_device.ringRotation >= 1.0f)
            s_device.ringRotation -= 1.0f;
        s_batch.vertexCount = 0;
        s_batch.texture = NULL;
        s_queues.colorCount = 0;
        s_queues.textCount[0] = 0;
        s_queues.textCount[1] = 0;
        s_queues.textCount[2] = 0;
        s_queues.panelCount = 0;
        s_queues.iconCount = 0;
        s_queues.highlightCount = 0;
        s_queues.overlayColorCount = 0;
        s_queues.overlayPanelCount = 0;
        s_queues.overlayHighlightCount = 0;
    }

    unsigned IconQuads()
    {
        return (unsigned)s_queues.iconCount;
    }

} /* namespace Gfx */
