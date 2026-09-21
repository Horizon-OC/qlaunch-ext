/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <switch.h>

namespace app {

struct SelectedUserArgument {
    static constexpr u32 Magic = 0xC79497CA;

    u32 magic;
    u8 is_selected; // 1 = user preselected
    u8 pad[3];
    AccountUid uid;
    u8 unused[0x70];
};
static_assert(sizeof(SelectedUserArgument) == 0x88);

extern bool g_hasFocus;

bool IsActive();
Result Terminate();
Result Start(u64 app_id, bool system, const AccountUid &user_id);
bool HasForeground();
Result SetForeground();
Result Send(const void *data, size_t size, AppletLaunchParameterKind kind);
u64 GetId();

} // namespace app
