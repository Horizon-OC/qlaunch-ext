#include "logging.hpp"

namespace logging {

    constexpr kMaxBufSize = 128;

    LogOutput CurrentLogOutput;
    std::string path LogPath;
    FILE* fp;

    void Initialize() {
        CurrentLogOutput = LogOutput_None;
    }

    void Exit() {
        /* Close the file if nessesary */
        if(CurrentLogOutput == LogOutput_File) {
            fclose(&fp);
        }
        CurrentLogOutput = LogOutput_None;
    }

    void SetFileLoggingPath(std::string path) {
        LogPath = path;
    }
    
    void LogLine(const char* fmt, ...) {
        /* Variadic arguments */
        va_list ap;
        va_start(ap, fmt);

        if(CurrentLogOutput == LogOutput_File) {

            /* Open file and append to it once */
            if(fp == nullptr) {
                fp = fopen(path.c_str(), "a+");
            }

            /* Actually print to the file if it opened correctly */
            if(fp != nullptr) {
                vfprintf(&fp, fmt, ap);
            }
        } else if(CurrentLogOutput == LogOutput_UART) {
            char buf[kMaxBufSize] = { 0 };

            /* Populate the buffer*/
            vsnprintf(buf, kMaxBufSize, fmt, ap);

            /* Output to uart, accounting for null terminator*/
            svcOutputDebugString(buf, strlen(buf) + 1);
        }
        
        va_end(ap);
    }

    void SetLogOutput(LogOutput output) {
        CurrentLogOutput = output;
    }

    LogOutput GetLogOutput() {
        return CurrentLogOutput;
    }

    std::string GetCurrentLogPath() {
        return LogPath;
    }

}