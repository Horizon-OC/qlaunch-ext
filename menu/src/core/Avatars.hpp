/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once

namespace Avatars {
    /// @brief Refresh cached avatar images from accounts.
    void Refresh();

    /// @brief Number of cached avatars.
    /// @return Avatar count.
    int Count();

    /// @brief Look up the icon slot for an avatar.
    /// @param i Avatar index.
    /// @return HUD icon slot, or <= 0 when missing.
    int Slot(int i);
} // namespace Avatars
