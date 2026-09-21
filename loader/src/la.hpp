/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

namespace la {

enum class HomeApplet : int {
    Album = 0,
    Controllers = 1,
    MiiEdit = 2,
};
constexpr int kHomeAppletCount = 3;

bool IsActive();
Result Terminate();
Result Start(AppletId id, s32 version);
Result Start(AppletId id, s32 version, const void *a, size_t na);
Result Start(AppletId id, s32 version,
             const void *a, size_t na, const void *b, size_t nb);

Result OpenAlbum();
Result OpenControllers();
Result OpenMiiEdit();
Result OpenHomeApplet(HomeApplet which);

} // namespace la
