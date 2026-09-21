/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "qext.hpp"
#include "app.hpp"
#include "la.hpp"
#include "titles.hpp"
#include <shared/logging.hpp>

namespace {

AccountUid PickUser()
{
    AccountUid uid{};
    if (R_SUCCEEDED(accountGetLastOpenedUser(&uid)) && accountUidIsValid(&uid))
        return uid;
    AccountUid list[8] = {};
    s32 total = 0;
    if (R_SUCCEEDED(accountListAllUsers(list, 8, &total))) {
        for (s32 i = 0; i < total && i < 8; i++) {
            if (accountUidIsValid(&list[i]))
                return list[i];
        }
    }
    AccountUid empty{};
    return empty;
}

} // namespace

extern "C" int qext_refresh_titles(void) { return titles::Refresh(); }
extern "C" int qext_title_count(void) { return titles::Count(); }
extern "C" u64 qext_title_id(int i) { return titles::Id(i); }
extern "C" int qext_title_name(int i, char *o, unsigned c) { return titles::Name(i, o, c); }
extern "C" int qext_title_icon_size(int i) { return titles::IconSize(i); }
extern "C" int qext_title_icon(int i, void *o, unsigned c) { return titles::Icon(i, o, c); }

extern "C" Result qext_launch_title(u64 tid)
{
    if (!tid)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    if (app::IsActive()) {
        /* Asking is for the menu */
        logging::LogLine("[qext] replacing running %016llX",
                         (unsigned long long)app::GetId());
        Result rc = app::Terminate();
        if (R_FAILED(rc))
            return rc;
    }
    AccountUid uid = PickUser();
    logging::LogLine("[qext] launching %016llX", (unsigned long long)tid);
    return app::Start(tid, false, uid);
}

extern "C" Result qext_resume_game(void)
{
    if (!app::IsActive())
        return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    appletUnlockForeground();
    return app::SetForeground();
}

extern "C" Result qext_launch_applet(int kind)
{
    if (kind < 0 || kind >= la::kHomeAppletCount)
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    return la::OpenHomeApplet((la::HomeApplet)kind);
}

extern "C" Result qext_terminate_game(void)
{
    if (!app::IsActive())
        return 0;
    return app::Terminate();
}

extern "C" int qext_game_running(void) { return app::IsActive() ? 1 : 0; }
extern "C" int qext_game_has_foreground(void) { return app::HasForeground() ? 1 : 0; }
extern "C" u64 qext_suspended_title(void) { return app::IsActive() ? app::GetId() : 0; }
