#pragma once
#include <cstdarg>
#include <cstdio>
#include <switch.h>

namespace logging {
    enum LogOutput {
        LogOutput_None = 0,
        LogOutput_File,
        LogOutput_UART,
    };

    void Initialize();
    void Exit();
    void SetFileLoggingPath(const char *path);
    void LogLine(const char *fmt, ...);
    void SetLogOutput(LogOutput output);
    LogOutput GetLogOutput();
    const char *GetCurrentLogPath();

} // namespace logging
