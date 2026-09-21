#include "logging.hpp"
#include <cstring>

namespace logging {

static constexpr size_t kMaxBufSize = 256;
static constexpr size_t kMaxPathSize = 256;
static LogOutput CurrentLogOutput = LogOutput_None;
static char LogPath[kMaxPathSize] = {0};
static FILE *fp = nullptr;

void Initialize() {
    CurrentLogOutput = LogOutput_None;
    LogPath[0] = 0;
    fp = nullptr;
}

void Exit() {
    if (fp != nullptr) {
        fclose(fp);
        fp = nullptr;
    }
    CurrentLogOutput = LogOutput_None;
}

void SetFileLoggingPath(const char *path) {
    if (path == nullptr) {
        LogPath[0] = 0;
        return;
    }
    strncpy(LogPath, path, sizeof(LogPath) - 1);
    LogPath[sizeof(LogPath) - 1] = 0;
}

void LogLine(const char *fmt, ...) {
    char buf[kMaxBufSize];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (CurrentLogOutput == LogOutput_File) {
        if (fp == nullptr && LogPath[0] != 0) {
            fp = fopen(LogPath, "w");
        }
        if (fp != nullptr) {
            fputs(buf, fp);
            fputc(10, fp);
            fflush(fp);
            return;
        }
        /* SD not ready */
    }
    if (CurrentLogOutput != LogOutput_None) {
        svcOutputDebugString(buf, strlen(buf));
    }
}

void SetLogOutput(LogOutput output) {
    CurrentLogOutput = output;
}

LogOutput GetLogOutput() {
    return CurrentLogOutput;
}

const char *GetCurrentLogPath() {
    return LogPath;
}

} // namespace logging
