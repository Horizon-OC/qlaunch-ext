/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */
#include "sys.hpp"
#include "app.hpp"
#include <shared/logging.hpp>

namespace sys {

bool HasForeground()
{
    return !app::g_hasFocus;
}

Result SetForeground()
{
    Result rc = appletRequestToGetForeground();
    if (R_FAILED(rc)) {
        logging::LogLine("[sys] request fg rc=0x%X", rc);
        return rc;
    }
    app::g_hasFocus = false;
    return 0;
}

void EnterSleep()
{
    Result rc = appletStartSleepSequence(true);
    logging::LogLine("[sys] sleep sequence rc=0x%X", rc);
}

} // namespace sys
