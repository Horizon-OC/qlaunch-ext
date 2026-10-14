/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

constexpr int FONT_ATLAS_W = 1024;
constexpr int FONT_ATLAS_H = 1024;

/* Texture slots / text buffers. */
constexpr int FONT_SLOT_LATIN = 0;
constexpr int FONT_SLOT_KANA = 1;
constexpr int FONT_SLOT_ICON = 2;

namespace Font {
    /// @brief Initialize font atlases and text buffers.
    /// @return True on success.
    bool Init();

    /// @brief Shut down font rendering.
    void Shutdown();

    /// @brief Reload system fonts (theme/content change).
    void Refresh();

    /// @brief Whether font rendering is usable.
    /// @return True when initialized.
    bool Ok();

    /// @brief Draw a string at a position.
    /// @param s UTF-8 text.
    /// @param x X position.
    /// @param y Baseline Y position.
    /// @param px Pixel size.
    /// @param r Red 0-1.
    /// @param g Green 0-1.
    /// @param b Blue 0-1.
    /// @return Advance width drawn.
    float Draw(const char *s, float x, float y, float px,
               float r, float g, float b);

    /// @brief Measure a string without drawing.
    /// @param s UTF-8 text.
    /// @param px Pixel size.
    /// @return Advance width.
    float Measure(const char *s, float px);

    /// @brief Draw a string centered on X with a max width.
    /// @param s UTF-8 text.
    /// @param cx Center X.
    /// @param y Baseline Y position.
    /// @param px Pixel size.
    /// @param maxW Maximum width before squeezing.
    /// @param r Red 0-1.
    /// @param g Green 0-1.
    /// @param b Blue 0-1.
    void Centered(const char *s, float cx, float y, float px, float maxW,
                  float r, float g, float b);

    /// @brief Decode the next codepoint and advance the cursor.
    /// @param pp Pointer to the UTF-8 cursor (updated).
    /// @return Codepoint, or 0 at end of string.
    unsigned Next(const char **pp);
} /* namespace Font */
