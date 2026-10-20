/* default layout touch navigation (C) 2026 Souldbminer */
/* Licensed under the GPLv2                          */
#pragma once
#include "LayoutApi.hpp"
#include <switch.h>

namespace TouchNav {

    /// @brief Interpret one touch frame into buttons, taps, drags, and press claims.
    /// Taps go to tabs, popups, manual submenu handlers, registered
    /// cells, or the A fallback; finger-down offers SubmenuTouchDown
    /// first and claimed fingers get SubmenuTouchUp on release.
    /// Drags scroll supporting tab content with inertia; fast horizontal
    /// swipes pulse Left/Right; swipe down exits the folder view.
    /// Ignored while transitioning.
    /// @param touches Touch frame (may be null when untouched).
    /// @param touchCount Touch point count.
    /// @param buttonsDown Proven pressed mask (pulses OR-ed in).
    void Handle(const LayoutTouch *touches, int touchCount, u64 &buttonsDown);

} /* namespace TouchNav */
