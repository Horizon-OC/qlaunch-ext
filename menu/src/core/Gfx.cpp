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

    constexpr int kMaxTex = GFX_MAX_TEX;
    constexpr int kCornerSeg = 6;
    constexpr int kRingEdgeSeg = 2;
    constexpr int kRingCornerSeg = 6;
    constexpr int kRingMaxPts = 64;
    constexpr int kTempMax = 512;

    static SDL_Window *s_win;
    static SDL_Renderer *s_ren;
    static bool s_live;
    static bool s_inited;

    static int s_fbW;
    static int s_fbH;
    static float s_s;
    static float s_ox;
    static float s_oy;
    static float s_ringRot;

    static SDL_Texture *s_Texture[kMaxTex];
    static int s_TextureW[kMaxTex];
    static int s_TextureH[kMaxTex];
    static bool s_TextureStream[kMaxTex];

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

    static CQ s_colorQ[GFX_VTX_MAX];
    static int s_colorN;
    static TQ s_TexturetQ[3][GFX_TEXT_MAX];
    static int s_TexturetN[3];
    static PQ s_panelQ[GFX_PANEL_MAX];
    static int s_panelN;
    static IQ s_iconQ[GFX_ICONQ_MAX];
    static int s_iconN;
    static HQ s_hlQ[GFX_HL_MAX];
    static int s_hlN;

    static SDL_Vertex s_tmp[kTempMax];

    static void Submit(SDL_Texture *tex, SDL_Vertex *v, int n);

    /* Batching */
    static SDL_Vertex s_run[4096];
    static int s_runN;
    static SDL_Texture *s_runTex;

    static void FlushRun() {
        if (s_runN > 0) {
            Submit(s_runTex, s_run, s_runN);
            s_runN = 0;
        }
    }

    static void EmitRun(SDL_Texture *tex, SDL_Vertex *v, int n) {
        int done = 0;
        if (n <= 0)
            return;
        if (tex != s_runTex)
            FlushRun();
        s_runTex = tex;
        while (done < n) {
            int room = 4096 - s_runN;
            int take = n - done;
            if (take > room)
                take = room;
            if (take <= 0) {
                FlushRun();
                continue;
            }
            memcpy(&s_run[s_runN], &v[done], (size_t)take * sizeof(SDL_Vertex));
            s_runN += take;
            done += take;
        }
    }

    /// @brief Fit the 1920x1080 design into the framebuffer, letterboxed.
    static void FitTransform() {
        s_s = (float)s_fbW / 1920.0f;
        float sy = (float)s_fbH / 1080.0f;
        if (sy < s_s)
            s_s = sy;
        s_ox = ((float)s_fbW - 1920.0f * s_s) * 0.5f;
        s_oy = ((float)s_fbH - 1080.0f * s_s) * 0.5f;
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
                         float x, float y, float w, float h, float rad,
                         float r, float g, float b, float a)
{
    int n = 0;
    float rr = rad;
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
            for (int s = 0; s < kCornerSeg; s++) {
                float t0 = a0[c] + 1.5707963f * (float)s / (float)kCornerSeg;
                float t1 = a0[c] + 1.5707963f * (float)(s + 1) / (float)kCornerSeg;
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
                            float rTL, float rTR, float rBR, float rBL,
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
    if (rTL < 0.0f)
        rTL = 0.0f;
    if (rTR < 0.0f)
        rTR = 0.0f;
    if (rBR < 0.0f)
        rBR = 0.0f;
    if (rBL < 0.0f)
        rBL = 0.0f;
    if (rTL > w * 0.5f)
        rTL = w * 0.5f;
    if (rTR > w * 0.5f)
        rTR = w * 0.5f;
    if (rBR > w * 0.5f)
        rBR = w * 0.5f;
    if (rBL > w * 0.5f)
        rBL = w * 0.5f;
    if (rTL + rBL > h) {
        float k = h / (rTL + rBL);
        rTL *= k;
        rBL *= k;
    }
    if (rTR + rBR > h) {
        float k = h / (rTR + rBR);
        rTR *= k;
        rBR *= k;
    }
    if (rTL <= 0.0f && rTR <= 0.0f && rBR <= 0.0f && rBL <= 0.0f) {
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
            for (int s = 1; s <= kCornerSeg; s++) {
                float t = a0 + 1.5707963f * (float)s / (float)kCornerSeg;
                push(ccx + cosf(t) * rr, ccy + sinf(t) * rr);
            }
        };
        push(x + rTL, y);
        push(x + w - rTR, y);
        arc(x + w - rTR, y + rTR, rTR, -1.5707963f);
        push(x + w, y + h - rBR);
        arc(x + w - rBR, y + h - rBR, rBR, 0.0f);
        push(x + rBL, y + h);
        arc(x + rBL, y + h - rBL, rBL, 1.5707963f);
        push(x, y + rTL);
        arc(x + rTL, y + rTL, rTL, 3.14159265f);
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
                        float x, float y, float w, float h, float rad, float thick,
                        const float *c0, const float *c1, const float *c2, const float *c3,
                        float rot)
{
    float R = rad, T = thick;
    float mn = w < h ? w : h;
    float Wi, Hi, Ri;
    struct RP { float ox, oy, ix, iy; };
    RP pts[kRingMaxPts];
    int npt = 0;
    float total = 0.0f;
    float acc[kRingMaxPts];
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
            if (npt >= kRingMaxPts)
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
            for (int s = 1; s <= kRingEdgeSeg; s++) {
                float t = (float)s / (float)kRingEdgeSeg;
                push(sx0 + (nx - sx0) * t, sy0 + (ny - sy0) * t,
                     six0 + (nix - six0) * t, siy0 + (niy - siy0) * t);
            }
            for (int s = 1; s <= kRingCornerSeg; s++) {
                float t = ea[c] + 1.5707963f * (float)s / (float)kRingCornerSeg;
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
    static void Submit(SDL_Texture *tex, SDL_Vertex *v, int n)
{
    if (n <= 0 || !s_ren)
        return;
    SDL_RenderGeometry(s_ren, tex, v, n, NULL, 0);
}

} /* anonymous namespace */

bool Init(NWindow *win, int fbW, int fbH)
{
    Destroy();
    s_fbW = fbW;
    s_fbH = fbH;
    FitTransform();

    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        Log::Line("[menu] SDL_Init failed");
        return false;
    }
    s_inited = true;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_WINDOW_CREATE_SWITCH_NWINDOW_POINTER, win);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, fbW);
    SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, fbH);
    s_win = SDL_CreateWindowWithProperties(props);
    SDL_DestroyProperties(props);
    if (!s_win) {
        Log::Line("[menu] window failed");
        Destroy();
        return false;
    }

    s_ren = SDL_CreateRenderer(s_win, "deko3d");
    if (!s_ren) {
        Log::Line("[menu] renderer failed");
        Destroy();
        return false;
    }
    SDL_SetRenderDrawBlendMode(s_ren, SDL_BLENDMODE_BLEND);

    ResetCounts();
    s_live = true;
    return true;
}

void Destroy()
{
    s_live = false;
    for (int i = 0; i < kMaxTex; i++) {
        if (s_Texture[i]) {
            SDL_DestroyTexture(s_Texture[i]);
            s_Texture[i] = NULL;
        }
        s_TextureW[i] = 0;
        s_TextureH[i] = 0;
        s_TextureStream[i] = false;
    }
    if (s_ren) {
        SDL_DestroyRenderer(s_ren);
        s_ren = NULL;
    }
    if (s_win) {
        SDL_DestroyWindow(s_win);
        s_win = NULL;
    }
    if (s_inited) {
        SDL_Quit();
        s_inited = false;
    }
}

bool Ready()
{
    return s_live;
}

bool BeginFrame()
{
    return s_live;
}

void DrawColor()
{
    int n = 0;
    for (int i = 0; i + 6 <= s_colorN; i += 6) {
        if (n + 6 > kTempMax) {
            EmitRun(NULL, s_tmp, n);
            n = 0;
        }
        for (int k = 0; k < 6; k++) {
            s_tmp[n].position.x = s_colorQ[i + k].x;
            s_tmp[n].position.y = s_colorQ[i + k].y;
            s_tmp[n].tex_coord.x = 0.0f;
            s_tmp[n].tex_coord.y = 0.0f;
            s_tmp[n].color.r = s_colorQ[i + k].r;
            s_tmp[n].color.g = s_colorQ[i + k].g;
            s_tmp[n].color.b = s_colorQ[i + k].b;
            s_tmp[n].color.a = s_colorQ[i + k].a;
            n++;
        }
    }
    EmitRun(NULL, s_tmp, n);
    FlushRun();
}

void DrawPanelsBg()
{
    int n = 0;
    if (s_panelN <= 0)
        return;
    n = TessRRect(s_tmp, kTempMax, s_panelQ[0].x, s_panelQ[0].y,
                  s_panelQ[0].w, s_panelQ[0].h, s_panelQ[0].rad,
                  s_panelQ[0].r, s_panelQ[0].g, s_panelQ[0].b, s_panelQ[0].a);
    EmitRun(NULL, s_tmp, n);
    FlushRun();
}

void DrawPanels()
{
    for (int i = 1; i < s_panelN; i++) {
        int n = TessRRect(s_tmp, kTempMax, s_panelQ[i].x, s_panelQ[i].y,
                          s_panelQ[i].w, s_panelQ[i].h, s_panelQ[i].rad,
                          s_panelQ[i].r, s_panelQ[i].g, s_panelQ[i].b, s_panelQ[i].a);
        EmitRun(NULL, s_tmp, n);
    }
    FlushRun();
}

void DrawHighlights()
{
    for (int i = 0; i < s_hlN; i++) {
        HQ *q = &s_hlQ[i];
        int n = TessRing(s_tmp, kTempMax, q->x, q->y, q->w, q->h, q->rad,
                         q->thick, q->c0, q->c1, q->c2, q->c3, q->rot);
        EmitRun(NULL, s_tmp, n);
    }
    FlushRun();
}

void DrawIcons()
{
    for (int i = 0; i < s_iconN; i++) {
        IQ *q = &s_iconQ[i];
        int n;
        if (!TexValid(q->slot) || q->slot >= kMaxTex || !s_Texture[q->slot])
            continue;
        n = TessRRectTex(s_tmp, kTempMax, q->x, q->y, q->w, q->h,
                         q->rTL, q->rTR, q->rBR, q->rBL,
                         q->u0, q->v0, q->u1, q->v1, q->r, q->g, q->b, q->a);
        EmitRun(s_Texture[q->slot], s_tmp, n);
    }
    FlushRun();
}

void DrawTextBuf(int texSlot, int buf)
{
    int n = 0;
    if (texSlot < 0 || texSlot >= kMaxTex || !s_Texture[texSlot])
        return;
    if (buf < 0 || buf > 2)
        return;
    for (int i = 0; i + 6 <= s_TexturetN[buf]; i += 6) {
        if (n + 6 > kTempMax) {
            EmitRun(s_Texture[texSlot], s_tmp, n);
            n = 0;
        }
        for (int k = 0; k < 6; k++) {
            s_tmp[n].position.x = s_TexturetQ[buf][i + k].x;
            s_tmp[n].position.y = s_TexturetQ[buf][i + k].y;
            s_tmp[n].tex_coord.x = s_TexturetQ[buf][i + k].u;
            s_tmp[n].tex_coord.y = s_TexturetQ[buf][i + k].v;
            s_tmp[n].color.r = s_TexturetQ[buf][i + k].r;
            s_tmp[n].color.g = s_TexturetQ[buf][i + k].g;
            s_tmp[n].color.b = s_TexturetQ[buf][i + k].b;
            s_tmp[n].color.a = s_TexturetQ[buf][i + k].a;
            n++;
        }
    }
    EmitRun(s_Texture[texSlot], s_tmp, n);
    FlushRun();
}

void EndFrame()
{
    if (s_live)
        SDL_RenderPresent(s_ren);
}

float X(float x)
{
    return s_ox + x * s_s;
}

float Y(float y)
{
    return s_oy + y * s_s;
}

float SC(float v)
{
    return v * s_s;
}

int TexAlloc()
{
    for (int i = 0; i < kMaxTex; i++) {
        if (!s_Texture[i] && s_TextureW[i] == 0) {
            s_TextureW[i] = -1;
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
    if (slot < 0 || slot >= kMaxTex)
        return;
    if (!s_Texture[slot] && s_TextureW[slot] == 0)
        return; // never allocated: stale handle, nothing to do
    if (s_Texture[slot]) {
        SDL_DestroyTexture(s_Texture[slot]);
        s_Texture[slot] = NULL;
    }
    s_TextureW[slot] = 0;
    s_TextureH[slot] = 0;
    s_TextureStream[slot] = false;
}

bool TexUpload(int slot, const uint8_t *px, int w, int h, TexFormat fmt)
{
    SDL_Rect rect;
    if (slot < 0 || slot >= kMaxTex || !px || w <= 0 || h <= 0 || !s_live)
        return false;
    if (s_Texture[slot] && (s_TextureW[slot] != w || s_TextureH[slot] != h || s_TextureStream[slot])) {
        SDL_DestroyTexture(s_Texture[slot]);
        s_Texture[slot] = NULL;
    }
    if (!s_Texture[slot]) {
        s_Texture[slot] = SDL_CreateTexture(s_ren, SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_STATIC, w, h);
        if (!s_Texture[slot])
            return false;
        s_TextureW[slot] = w;
        s_TextureH[slot] = h;
        s_TextureStream[slot] = false;
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
        ok = SDL_UpdateTexture(s_Texture[slot], &rect, tmp, w * 4);
        free(tmp);
        return ok;
    }
    return SDL_UpdateTexture(s_Texture[slot], &rect, px, w * 4);
}

bool TexLock(int slot, int w, int h, void **pixels, int *pitch)
{
    if (slot < 0 || slot >= kMaxTex || !pixels || !pitch || !s_live)
        return false;
    if (s_Texture[slot] && (s_TextureW[slot] != w || s_TextureH[slot] != h || !s_TextureStream[slot])) {
        SDL_DestroyTexture(s_Texture[slot]);
        s_Texture[slot] = NULL;
    }
    if (!s_Texture[slot]) {
        s_Texture[slot] = SDL_CreateTexture(s_ren, SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_STREAMING, w, h);
        if (!s_Texture[slot])
            return false;
        s_TextureW[slot] = w;
        s_TextureH[slot] = h;
        s_TextureStream[slot] = true;
    }
    return SDL_LockTexture(s_Texture[slot], NULL, pixels, pitch);
}

void TexUnlock(int slot)
{
    if (slot < 0 || slot >= kMaxTex || !s_Texture[slot])
        return;
    SDL_UnlockTexture(s_Texture[slot]);
}

void PushQuad(float x, float y, float w, float h,
              float r, float g, float b, float a)
{
    float X0 = X(x), Y0 = Y(y), X1 = X(x + w), Y1 = Y(y + h);
    CQ *v;
    if (s_colorN + 6 > GFX_VTX_MAX)
        return;
    v = &s_colorQ[s_colorN];
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
    s_colorN += 6;
}

void PushPanel(float x, float y, float w, float h, float rad,
               float r, float g, float b, float a)
{
    PQ *q;
    if (s_panelN + 1 > GFX_PANEL_MAX)
        return;
    if (w <= 0.0f || h <= 0.0f)
        return;
    q = &s_panelQ[s_panelN];
    q->x = X(x);
    q->y = Y(y);
    q->w = SC(w);
    q->h = SC(h);
    q->rad = SC(rad);
    q->r = r;
    q->g = g;
    q->b = b;
    q->a = a;
    s_panelN++;
}

unsigned PushIcon(float x, float y, float w, float h,
                  float rTL, float rTR, float rBR, float rBL,
                  float r, float g, float b, float border, int tex,
                  float u0, float v0, float u1, float v1, float edgeBlend, float alpha)
{
    IQ *q;
    unsigned id;
    (void)border;
    (void)edgeBlend;
    if (s_iconN + 1 > GFX_ICONQ_MAX)
        return 0xFFFFFFFFu;
    if (w <= 0.0f || h <= 0.0f)
        return 0xFFFFFFFFu;
    id = (unsigned)s_iconN;
    q = &s_iconQ[s_iconN];
    q->x = X(x);
    q->y = Y(y);
    q->w = SC(w);
    q->h = SC(h);
    q->rTL = SC(rTL);
    q->rTR = SC(rTR);
    q->rBR = SC(rBR);
    q->rBL = SC(rBL);
    q->u0 = u0;
    q->v0 = v0;
    q->u1 = u1;
    q->v1 = v1;
    q->r = r;
    q->g = g;
    q->b = b;
    q->a = alpha;
    q->slot = tex;
    s_iconN++;
    return id;
}

void PushSelectRing(float x, float y, float w, float h, float rad, float thick)
{
    static const float c0[4] = { 0.537f, 0.584f, 0.945f, 1.0f };
    static const float c1[4] = { 0.384f, 0.588f, 0.949f, 1.0f };
    static const float c2[4] = { 0.863f, 0.722f, 0.875f, 1.0f };
    static const float c3[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    PushHighlight(x, y, w, h, rad, thick, c0, c1, c2, c3, s_ringRot);
}

void PushHighlight(float x, float y, float w, float h, float rad,
                   float thick, const float *c0, const float *c1,
                   const float *c2, const float *c3, float rot)
{
    HQ *q;
    if (s_hlN + 1 > GFX_HL_MAX)
        return;
    if (w <= 0.0f || h <= 0.0f || thick <= 0.0f)
        return;
    q = &s_hlQ[s_hlN];
    q->x = X(x);
    q->y = Y(y);
    q->w = SC(w);
    q->h = SC(h);
    q->rad = SC(rad);
    q->thick = SC(thick);
    q->rot = rot;
    for (int i = 0; i < 4; i++) {
        q->c0[i] = c0[i];
        q->c1[i] = c1[i];
        q->c2[i] = c2[i];
        q->c3[i] = c3[i];
    }
    s_hlN++;
}

void PushText(int buf, float x0, float y0, float x1, float y1,
              float s0, float t0, float s1, float t1,
              float r, float g, float b)
{
    TQ *v;
    float X0 = X(x0), Y0 = Y(y0), X1 = X(x1), Y1 = Y(y1);
    if (buf < 0 || buf > 2 || s_TexturetN[buf] + 6 > GFX_TEXT_MAX)
        return;
    if (x1 <= x0 || y1 <= y0)
        return;
    v = &s_TexturetQ[buf][s_TexturetN[buf]];
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
        v[i].a = 1.0f;
    }
    s_TexturetN[buf] += 6;
}

void ResetCounts()
{
    s_ringRot += 0.5f / 60.0f;
    if (s_ringRot >= 1.0f)
        s_ringRot -= 1.0f;
    s_runN = 0;
    s_runTex = NULL;
    s_colorN = 0;
    s_TexturetN[0] = 0;
    s_TexturetN[1] = 0;
    s_TexturetN[2] = 0;
    s_panelN = 0;
    s_iconN = 0;
    s_hlN = 0;
}

unsigned IconQuads()
{
    return (unsigned)s_iconN;
}

} /* namespace Gfx */
