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
    void Reset() {
        counter = 0;
    }

    /// @brief Poll directional buttons and expand holds into downs.
    /// @param down Buttons pressed this frame (updated in place).
    /// @param held Buttons currently held.
    /// @param mask Direction mask to apply repeat to.
    /// @param spec Timing tunables.
    void Poll(u64 &down, u64 held, u64 mask, const RepeatSpec &spec) {
        u64 dirs = held & mask;

        if (down & mask) {
            counter = 0;
            return;
        }

        if (dirs) {
            counter++;

            if (counter > spec.holdFrames && (counter % spec.period) == 0) {
                down |= dirs;
            }
        } else {
            counter = 0;
        }
    }
};

/// @brief Default repeat timing (matches the legacy widgets).
inline RepeatSpec DefaultRepeat()
{
    RepeatSpec spec;
    spec.holdFrames = 20;
    spec.period = 6;
    return spec;
}

/// @brief Faster repeat timing for the home strip.
/// @return Spec with the default hold delay and a shorter repeat period.
inline RepeatSpec StripRepeat()
{
    RepeatSpec spec;
    spec.holdFrames = 20;
    spec.period = 5;
    return spec;
}
