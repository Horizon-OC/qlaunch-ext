/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "PadIcon.hpp"
#include <switch.h>

namespace PadIcon {

    namespace {

        /// @brief All mutable pad icon state, single global.
        struct PadIconState {
            unsigned glyphCodepoint;
            float tintRed;
            float tintGreen;
            float tintBlue;
            unsigned tickCount;
            bool isReady;
        };

        static PadIconState s_state = { 0xE122, 0.55f, 0.55f, 0.60f, 0, false };

    } // namespace

    /// @brief Decode a packed controller color into floats.
    /// @param red Red output.
    /// @param green Green output.
    /// @param blue Blue output.
    /// @param packedColor Packed 0xRRGGBBAA color.
    static void DecodePackedColor(float *red, float *green, float *blue, u32 packedColor)
    {
        if (packedColor == 0) {
            return;
        }

        float decodedRed = (float)((packedColor >> 24) & 0xFF) / 255.0f;
        float decodedGreen = (float)((packedColor >> 16) & 0xFF) / 255.0f;
        float decodedBlue = (float)((packedColor >> 8) & 0xFF) / 255.0f;

        *red = decodedRed;
        *green = decodedGreen;
        *blue = decodedBlue;
    }

    /// @brief Poll the controller style set and cache glyph + color.
    static void Refresh()
    {
        u32 styleSet = hidGetNpadStyleSet(HidNpadIdType_No1);

        if (styleSet == 0) {
            styleSet = hidGetNpadStyleSet(HidNpadIdType_Handheld);
        }

        unsigned glyphCodepoint = 0xE122;

        if (styleSet & HidNpadStyleTag_NpadHandheld) {
            glyphCodepoint = 0xE121;
        } else if (styleSet & HidNpadStyleTag_NpadJoyDual) {
            glyphCodepoint = 0xE122;
        } else if (styleSet & HidNpadStyleTag_NpadJoyLeft) {
            glyphCodepoint = (styleSet & HidNpadStyleTag_NpadJoyRight) ? 0xE122 : 0xE123;
        } else if (styleSet & HidNpadStyleTag_NpadJoyRight) {
            glyphCodepoint = 0xE124;
        } else if (styleSet & HidNpadStyleTag_NpadFullKey) {
            glyphCodepoint = 0xE12C;
        } else if (styleSet & HidNpadStyleTag_NpadGc) {
            glyphCodepoint = 0xE12C; /* No GC font */
        } else if (styleSet & HidNpadStyleTag_NpadPalma) {
            glyphCodepoint = 0xE122; /* No ball font */
        } else if (styleSet & HidNpadStyleTag_NpadLark) {
            glyphCodepoint = 0xE127;
        } else if (styleSet & HidNpadStyleTag_NpadHandheldLark) {
            glyphCodepoint = 0xE127;
        } else if (styleSet & HidNpadStyleTag_NpadLucia) {
            glyphCodepoint = 0xE128;
        } else if (styleSet & HidNpadStyleTag_NpadLagon) {
            glyphCodepoint = 0xE12C; /* No N64 glyph */
        } else if (styleSet & HidNpadStyleTag_NpadLager) {
            glyphCodepoint = 0xE129;
        } else if (styleSet & (HidNpadStyleTag_NpadSystemExt | HidNpadStyleTag_NpadSystem)) {
            glyphCodepoint = 0xE12C;
        } else if (styleSet == 0) {
            glyphCodepoint = 0xE122;
        }

        float tintRed = 0.55f;
        float tintGreen = 0.55f;
        float tintBlue = 0.60f;
        HidNpadControllerColor leftColor{};
        HidNpadControllerColor rightColor{};
        HidNpadControllerColor singleColor{};
        bool hasSplitColors = false;
        bool hasSingleColor = false;

        if (R_SUCCEEDED(hidGetNpadControllerColorSplit(HidNpadIdType_No1, &leftColor, &rightColor))) {
            if (leftColor.main != 0 || rightColor.main != 0) {
                hasSplitColors = true;
            }
        }

        if (R_SUCCEEDED(hidGetNpadControllerColorSingle(HidNpadIdType_No1, &singleColor))) {
            if (singleColor.main != 0) {
                hasSingleColor = true;
            }
        }

        if (hasSplitColors) {
            /* TODO: Do dual joycon properly */
            u32 packedColor = leftColor.main ? leftColor.main : rightColor.main;
            DecodePackedColor(&tintRed, &tintGreen, &tintBlue, packedColor);
        } else if (hasSingleColor) {
            DecodePackedColor(&tintRed, &tintGreen, &tintBlue, singleColor.main);
        }

        s_state.glyphCodepoint = glyphCodepoint;
        s_state.tintRed = tintRed;
        s_state.tintGreen = tintGreen;
        s_state.tintBlue = tintBlue;
        s_state.isReady = true;
    }

    /// @brief Poll connected styles into the cached pad icon.
    void Tick()
    {
        /* Poll colors slowly. */
        s_state.tickCount++;

        if (!s_state.isReady || (s_state.tickCount % 30) == 1) {
            Refresh();
        }
    }

    /// @brief Give the glyph for the current controller type.
    /// @return Glyph codepoint.
    unsigned Glyph()
    {
        if (!s_state.isReady) {
            Refresh();
        }

        return s_state.glyphCodepoint;
    }

    /// @brief Give the tint for the current controller type.
    /// @param red Red output.
    /// @param green Green output.
    /// @param blue Blue output.
    void Color(float *red, float *green, float *blue)
    {
        if (!s_state.isReady) {
            Refresh();
        }

        if (red) {
            *red = s_state.tintRed;
        }

        if (green) {
            *green = s_state.tintGreen;
        }

        if (blue) {
            *blue = s_state.tintBlue;
        }
    }

} // namespace PadIcon
