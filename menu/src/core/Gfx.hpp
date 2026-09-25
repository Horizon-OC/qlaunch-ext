/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>
#include <deko3d.h>

struct HomeVtx { float x, y, r, g, b, a; };
struct TextVtx { float x, y, u, v, r, g, b, a; };
struct PanelVtx {
    float x, y, ox, oy, w, h, rad, pad, r, g, b, a;
};
struct IconVtx {
    float x, y, u, v, ox, oy, w, h, rad, pad, r, g, b, a, tslot, brad;
};
struct HighlightVtx {
    float x, y, ox, oy, w, h, rad, thick;
    float c0r, c0g, c0b, c0a, c1r, c1g, c1b, c1a;
    float c2r, c2g, c2b, c2a, c3r, c3g, c3b, c3a;
    float rot;
};

enum {
    GFX_FB_NUM = 2,
    GFX_VTX_MAX = 2048,
    GFX_TEXT_MAX = 8192,
    GFX_PANEL_MAX = 2048,
    GFX_ICONQ_MAX = 2048,
    GFX_HL_MAX = 64,

    GFX_MAX_TEX = 256,
    GFX_CODE_SIZE = 64 * 1024,
    GFX_CMD_SIZE = 64 * 1024,
};

namespace Gfx {
    bool Init(NWindow *win, int fbW, int fbH);
    void Destroy();
    bool Ready();

    bool BeginFrame();
    void DrawColor();
    void DrawPanelsBg();
    void DrawPanels();
    void DrawHighlights();
    void DrawIcons();

    void EndFrame();
    void WaitIdle();

    float X(float x);
    float Y(float y);
    float SC(float v);

    int TexAlloc();
    void TexFree(int slot);
    void WriteImageDesc(int slot, const DkImageView *view);
    void WriteSamplerDesc();
    void ClearDescs();

    void PushQuad(float x, float y, float w, float h,
                  float r, float g, float b, float a);
    void PushPanel(float x, float y, float w, float h, float rad,
                   float r, float g, float b, float a);
    unsigned PushIcon(float x, float y, float w, float h, float rad,
                      float r, float g, float b, float border, int tex, float brad = -1.0f,
                      float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f);
    void PushSelectRing(float x, float y, float w, float h, float rad, float thick);
    void PushHighlight(float x, float y, float w, float h, float rad,
                       float thick, const float *c0, const float *c1,
                       const float *c2, const float *c3, float rot);
    void PushText(int buf, float x0, float y0, float x1, float y1,
                  float s0, float t0, float s1, float t1,
                  float r, float g, float b);
    void DrawTextBuf(int texSlot, int buf);

    void ResetCounts();

    unsigned IconQuads();
    DkDevice Device();

bool LoadShaders();
    void BindPanelAttribs();
    void BindHlAttribs();
    void BlendedState(DkRasterizerState *rast, DkColorWriteState *colW,
                      DkColorState *tcol, DkBlendState *bl);
    void BindTexSets();
    void BindUbo();

} /* namespace Gfx */