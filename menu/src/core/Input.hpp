/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

/// @brief Tunables for key-repeat behaviour.
struct RepeatSpec {
    unsigned holdFrames;
    unsigned period;
};

/// @brief Tiny state machine for directional key repeat.
struct InputRepeat {
    unsigned counter;

    /// @brief Reset the repeat counter.
    void Reset()
    {
        counter = 0;
    }

    /// @brief Poll directional buttons and expand holds into presses.
    /// @param buttonsDown Buttons pressed this frame (updated in place).
    /// @param buttonsHeld Buttons currently held.
    /// @param directionMask Direction mask to apply repeat to.
    /// @param repeatSpec Timing tunables.
    void Poll(u64 &buttonsDown, u64 buttonsHeld, u64 directionMask, const RepeatSpec &repeatSpec)
    {
        u64 heldDirections = buttonsHeld & directionMask;

        if (buttonsDown & directionMask) {
            counter = 0;
            return;
        }

        if (heldDirections) {
            counter++;

            if (counter > repeatSpec.holdFrames && (counter % repeatSpec.period) == 0) {
                buttonsDown |= heldDirections;
            }
        } else {
            counter = 0;
        }
    }
};

/// @brief Default repeat timing (matches the legacy widgets).
inline RepeatSpec DefaultRepeat()
{
    RepeatSpec repeatSpec;
    repeatSpec.holdFrames = 20;
    repeatSpec.period = 6;
    return repeatSpec;
}

/// @brief Faster repeat timing for the home strip.
/// @return Spec with the default hold delay and a shorter repeat period.
inline RepeatSpec StripRepeat()
{
    RepeatSpec repeatSpec;
    repeatSpec.holdFrames = 20;
    repeatSpec.period = 5;
    return repeatSpec;
}
