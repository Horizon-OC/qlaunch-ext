/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering...         */

#pragma once
#include <switch.h>
#include <stdint.h>

constexpr int GFX_VTX_MAX = 2048;
constexpr int GFX_TEXT_MAX = 8192;
constexpr int GFX_PANEL_MAX = 2048;
constexpr int GFX_ICONQ_MAX = 2048;
constexpr int GFX_HL_MAX = 64;

constexpr int GFX_OVCOLOR_MAX = 144;
constexpr int GFX_OVPANEL_MAX = 32;
constexpr int GFX_OVHL_MAX = 16;

constexpr int GFX_MAX_TEX = 256;

namespace Gfx {

    /// @brief Pixel source format for TexUpload.
    enum class TexFormat {
        RGBA, /* 32-bit RGBA bytes, native GPU layout. */
        R8,   /* 8-bit gray, expanded to white-with-alpha on upload. */
    };

    /// @brief Initialize SDL video + the deko3d renderer on an adopted window.
    /// @param win Host native window handle (adopted, never owned).
    /// @param fbW Framebuffer width.
    /// @param fbH Framebuffer height.
    /// @return True on success.
    bool Init(NWindow *win, int fbW, int fbH);

    /// @brief Destroy the renderer and free GPU resources.
    void Destroy();

    /// @brief Whether the renderer is usable.
    /// @return True when initialized.
    bool Ready();

    /// @brief Begin a frame (queues are submitted by the Draw* calls).
    /// @return False when the frame must be skipped.
    bool BeginFrame();

    /// @brief Render queued solid-color quads.
    void DrawColor();

    /// @brief Render the fullscreen background quad.
    void DrawPanelsBg();

    /// @brief Render queued rounded panels.
    void DrawPanels();

    /// @brief Render queued highlight rings.
    void DrawHighlights();

    /// @brief Render queued icon quads.
    void DrawIcons();

    /// @brief Render queued overlay quads, panels, and rings above icons.
    void DrawOverlay();

    /// @brief Submit the frame and present it.
    void EndFrame();

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

    /// @brief Allocate a texture slot (0..255, first free).
    /// Slots are engine-managed: validity is allocation state, never a
    /// hardcoded range. Owners hold -1 when they own nothing.
    /// @return Slot index, or < 0 when exhausted.
    int TexAlloc();

    /// @brief Whether a slot handle is usable (the single validity rule;
    /// use this instead of comparing against any range).
    /// @param slot Slot handle (< 0 means no texture).
    /// @return True when the handle refers to a texture.
    bool TexValid(int slot);

    /// @brief Free a texture slot. No-op unless currently allocated, so
    /// stale or never-allocated handles are always safe to drop.
    /// @param slot Slot from TexAlloc().
    void TexFree(int slot);

    /// @brief Create (or recreate on size change) and fully upload a texture.
    /// @param slot Descriptor slot.
    /// @param px Pixel data (RGBA bytes, or R8 gray for TexFormat::R8).
    /// @param w Width.
    /// @param h Height.
    /// @param fmt Pixel format.
    /// @return True on success.
    bool TexUpload(int slot, const uint8_t *px, int w, int h, TexFormat fmt);

    /// @brief Lock a streaming texture for CPU writes (creates it sized).
    /// @param slot Descriptor slot.
    /// @param w Width.
    /// @param h Height.
    /// @param pixels Output CPU pointer.
    /// @param pitch Output row pitch in bytes.
    /// @return True on success.
    bool TexLock(int slot, int w, int h, void **pixels, int *pitch);

    /// @brief Unlock a locked texture.
    /// @param slot Descriptor slot.
    void TexUnlock(int slot);

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
    /// @param rTL Top-left corner radius.
    /// @param rTR Top-right corner radius.
    /// @param rBR Bottom-right corner radius.
    /// @param rBL Bottom-left corner radius.
    /// @param r Red 0-1.
    /// @param g Green 0-1.
    /// @param b Blue 0-1.
    /// @param border Unused (old SDF border factor).
    /// @param tex Texture slot.
    /// @param u0 Left UV.
    /// @param v0 Top UV.
    /// @param u1 Right UV.
    /// @param v1 Bottom UV.
    /// @param edgeBlend Unused (old SDF edge factor).
    /// @param alpha Opacity multiplier.
    /// @return Queued quad count.
    unsigned PushIcon(float x, float y, float w, float h,
                      float rTL, float rTR, float rBR, float rBL,
                      float r, float g, float b, float border, int tex,
                      float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f, float edgeBlend = 1.0f, float alpha = 1.0f);

    /// @brief Queue a selection ring around a rect.
    /// @param x X position.
    /// @param y Y position.
    /// @param w Width.
    /// @param h Height.
    /// @param rad Corner radius.
    /// @param thick Ring thickness.
    void PushSelectRing(float x, float y, float w, float h, float rad, float thick);

    /// @brief Queue a solid-color quad in the overlay layer (modal dim).
    /// @param x X position.
    /// @param y Y position.
    /// @param w Width.
    /// @param h Height.
    /// @param r Red 0-1.
    /// @param g Green 0-1.
    /// @param b Blue 0-1.
    /// @param a Alpha 0-1.
    void PushOverlayQuad(float x, float y, float w, float h,
                         float r, float g, float b, float a);

    /// @brief Queue a rounded panel in the overlay layer (modal chrome).
    /// @param x X position.
    /// @param y Y position.
    /// @param w Width.
    /// @param h Height.
    /// @param rad Corner radius.
    /// @param r Red 0-1.
    /// @param g Green 0-1.
    /// @param b Blue 0-1.
    /// @param a Alpha 0-1.
    void PushOverlayPanel(float x, float y, float w, float h, float rad,
                          float r, float g, float b, float a);

    /// @brief Queue a selection ring in the overlay layer (modal focus).
    /// @param x X position.
    /// @param y Y position.
    /// @param w Width.
    /// @param h Height.
    /// @param rad Corner radius.
    /// @param thick Ring thickness.
    void PushOverlayRing(float x, float y, float w, float h, float rad, float thick);

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
    /// @param a Alpha 0-1.
    void PushText(int buf, float x0, float y0, float x1, float y1,
                  float s0, float t0, float s1, float t1,
                  float r, float g, float b, float a = 1.0f);

    /// @brief Render a text buffer through a texture slot.
    /// @param texSlot Texture slot.
    /// @param buf Text buffer id.
    void DrawTextBuf(int texSlot, int buf);

    /// @brief Reset per-frame queue counts.
    void ResetCounts();

    /// @brief Give the queued icon quad count.
    /// @return Quad count.
    unsigned IconQuads();

} /* namespace Gfx */
