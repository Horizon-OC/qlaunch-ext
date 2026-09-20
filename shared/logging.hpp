#include <switch.h>

namespace logging {
    
    enum LogOutput {
        LogOutput_File,
        LogOutput_UART,
    }

    void Initialize();
    void LogLine(const char* fmt, ...);

    void SetLogOutput(LogOutput output);

    LogOutput GetLogOutput();
}