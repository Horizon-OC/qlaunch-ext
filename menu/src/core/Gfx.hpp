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
    float x, y, u, v, ox, oy, w, h, rad, pad, r, g, b, a;
};

enum {
    GFX_FB_NUM = 2,
    GFX_VTX_MAX = 2048,
    GFX_TEXT_MAX = 8192,
    GFX_PANEL_MAX = 2048,
    GFX_ICONQ_MAX = 2048,
    GFX_MAX_TEX = 68, /* 0 latin, 1 kana, 2 iconfont, rest game icons */
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
    void DrawIcons(const int *slots);

    void EndFrame();
    void WaitIdle();

    float X(float x);
    float Y(float y);
    float SC(float v);

    void WriteImageDesc(int slot, const DkImageView *view);
    void WriteSamplerDesc();
    void ClearDescs();

    void PushQuad(float x, float y, float w, float h,
                  float r, float g, float b, float a);
    void PushPanel(float x, float y, float w, float h, float rad,
                   float r, float g, float b, float a);
    unsigned PushIcon(float x, float y, float w, float h, float rad,
                      float r, float g, float b);
    void PushText(int buf, float x0, float y0, float x1, float y1,
                  float s0, float t0, float s1, float t1,
                  float r, float g, float b);
    void DrawTextBuf(int texSlot, int buf);

    void ResetCounts();

    unsigned IconQuads();
    DkDevice Device();

bool LoadShaders();
    void BindPanelAttribs();
    void BlendedState(DkRasterizerState *rast, DkColorWriteState *colW,
                      DkColorState *tcol, DkBlendState *bl);
    void BindTexSets();
    void BindUbo();

} /* namespace Gfx */

