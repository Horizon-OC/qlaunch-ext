/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#include "Log.hpp"
#include "Qext.hpp"
#include <stdarg.h>
#include <stdio.h>

namespace Log {

void Line(const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    logging::LogLine("%s", buf);
}

} /* namespace Log */

