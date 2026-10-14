/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

namespace PadIcon {
    /// @brief Poll connected styles into the cached pad icon.
    void Tick();

    /// @brief Give the glyph for the current controller type.
    /// @return Glyph codepoint.
    unsigned Glyph();

    /// @brief Give the tint for the current controller type.
    /// @param r Red output.
    /// @param g Green output.
    /// @param b Blue output.
    void Color(float *r, float *g, float *b);
} // namespace PadIcon
