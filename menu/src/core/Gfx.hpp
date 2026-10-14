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
    float x, y, u, v, ox, oy, w, h, rad, pad, r, g, b, a, tslot, brad, edge;
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
    /// @brief Initialize the renderer and framebuffers.
    /// @param win Native window handle.
    /// @param fbW Framebuffer width.
    /// @param fbH Framebuffer height.
    /// @return True on success.
    bool Init(NWindow *win, int fbW, int fbH);

    /// @brief Destroy the renderer and free GPU resources.
    void Destroy();

    /// @brief Whether the renderer is usable.
    /// @return True when initialized.
    bool Ready();

    /// @brief Begin a frame and map transient buffers.
    /// @return False when the frame must be skipped.
    bool BeginFrame();

    /// @brief Render queued solid-color quads.
    void DrawColor();

    /// @brief Render panel background clears.
    void DrawPanelsBg();

    /// @brief Render queued rounded panels.
    void DrawPanels();

    /// @brief Render queued highlight rings.
    void DrawHighlights();

    /// @brief Render queued icon quads.
    void DrawIcons();

    /// @brief Submit the frame and present it.
    void EndFrame();

    /// @brief Wait for the GPU to go idle.
    void WaitIdle();

    /// @brief Scale an X coordinate to framebuffer space.
    /// @param x Design X (1920-wide).
    /// @return Framebuffer X.
    float X(float x);

    /// @brief Scale a Y coordinate to framebuffer space.
    /// @param y Design Y (1080-high).
    /// @return Framebuffer Y.
    float Y(float y);

    /// @brief Scale a scalar to framebuffer space.
    /// @param v Design value.
    /// @return Framebuffer value.
    float SC(float v);

    /// @brief Allocate a texture descriptor slot.
    /// @return Slot index, or < 0 when exhausted.
    int TexAlloc();

    /// @brief Free a texture descriptor slot.
    /// @param slot Slot from TexAlloc().
    void TexFree(int slot);

    /// @brief Write an image descriptor into a slot.
    /// @param slot Descriptor slot.
    /// @param view Image view to bind.
    void WriteImageDesc(int slot, const DkImageView *view);

    /// @brief Write the shared sampler descriptor.
    void WriteSamplerDesc();

    /// @brief Clear all texture descriptors.
    void ClearDescs();

    /// @brief Queue a solid-color quad.
    /// @param x X position.
    /// @param y Y position.
    /// @param w Width.
    /// @param h Height.
    /// @param r Red 0-1.
    /// @param g Green 0-1.
    /// @param b Blue 0-1.
    /// @param a Alpha 0-1.
    void PushQuad(float x, float y, float w, float h,
                  float r, float g, float b, float a);

    /// @brief Queue a rounded panel.
    /// @param x X position.
    /// @param y Y position.
    /// @param w Width.
    /// @param h Height.
    /// @param rad Corner radius.
    /// @param r Red 0-1.
    /// @param g Green 0-1.
    /// @param b Blue 0-1.
    /// @param a Alpha 0-1.
    void PushPanel(float x, float y, float w, float h, float rad,
                   float r, float g, float b, float a);

    /// @brief Queue an icon quad bound to a texture slot.
    /// @param x X position.
    /// @param y Y position.
    /// @param w Width.
    /// @param h Height.
    /// @param rad Corner radius.
    /// @param r Red 0-1.
    /// @param g Green 0-1.
    /// @param b Blue 0-1.
    /// @param border Border blend factor.
    /// @param tex Texture slot.
    /// @param brad Bottom radius override (-1 keeps rad).
    /// @param u0 Left UV.
    /// @param v0 Top UV.
    /// @param u1 Right UV.
    /// @param v1 Bottom UV.
    /// @param edgeBlend Edge blend factor.
    /// @param alpha Opacity multiplier.
    /// @return Queued quad count.
    unsigned PushIcon(float x, float y, float w, float h, float rad,
                      float r, float g, float b, float border, int tex, float brad = -1.0f,
                      float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f, float edgeBlend = 1.0f, float alpha = 1.0f);

    /// @brief Queue a selection ring around a rect.
    /// @param x X position.
    /// @param y Y position.
    /// @param w Width.
    /// @param h Height.
    /// @param rad Corner radius.
    /// @param thick Ring thickness.
    void PushSelectRing(float x, float y, float w, float h, float rad, float thick);

    /// @brief Queue a four-color highlight ring.
    /// @param x X position.
    /// @param y Y position.
    /// @param w Width.
    /// @param h Height.
    /// @param rad Corner radius.
    /// @param thick Ring thickness.
    /// @param c0 First corner color (RGBA).
    /// @param c1 Second corner color (RGBA).
    /// @param c2 Third corner color (RGBA).
    /// @param c3 Fourth corner color (RGBA).
    /// @param rot Rotation phase.
    void PushHighlight(float x, float y, float w, float h, float rad,
                       float thick, const float *c0, const float *c1,
                       const float *c2, const float *c3, float rot);

    /// @brief Queue a textured text quad.
    /// @param buf Text buffer id.
    /// @param x0 Left X.
    /// @param y0 Top Y.
    /// @param x1 Right X.
    /// @param y1 Bottom Y.
    /// @param s0 Left U.
    /// @param t0 Top V.
    /// @param s1 Right U.
    /// @param t1 Bottom V.
    /// @param r Red 0-1.
    /// @param g Green 0-1.
    /// @param b Blue 0-1.
    void PushText(int buf, float x0, float y0, float x1, float y1,
                  float s0, float t0, float s1, float t1,
                  float r, float g, float b);

    /// @brief Render a text buffer through a texture slot.
    /// @param texSlot Texture slot.
    /// @param buf Text buffer id.
    void DrawTextBuf(int texSlot, int buf);

    /// @brief Reset per-frame queue counts.
    void ResetCounts();

    /// @brief Give the queued icon quad count.
    /// @return Quad count.
    unsigned IconQuads();

    /// @brief Give the deko3d device.
    /// @return Device handle.
    DkDevice Device();

    /// @brief Load compiled shaders into GPU memory.
    /// @return True on success.
    bool LoadShaders();

    /// @brief Bind panel vertex attributes.
    void BindPanelAttribs();

    /// @brief Bind highlight vertex attributes.
    void BindHlAttribs();

    /// @brief Build the blended rasterizer/color state.
    /// @param rast Rasterizer state output.
    /// @param colW Color-write state output.
    /// @param tcol Fragment color state output.
    /// @param bl Blend state output.
    void BlendedState(DkRasterizerState *rast, DkColorWriteState *colW,
                      DkColorState *tcol, DkBlendState *bl);

    /// @brief Bind texture descriptor sets.
    void BindTexSets();

    /// @brief Bind the shared uniform buffer.
    void BindUbo();

} /* namespace Gfx */
