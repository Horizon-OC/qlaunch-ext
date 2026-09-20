#pragma once
#include <switch.h>
#include <string>
#include <cstdio>
#include <cstdarg>

namespace logging {
    
    /// @brief Where to log
    enum LogOutput {
        LogOutput_None = 0,
        LogOutput_File,
        LogOutput_UART,
    }

    /// @brief Initialize the logger
    void Initialize();

    /// @brief Exit the logger
    void Exit();

    /// @brief Set the path where file logging should go
    /// @param path Where to write lines
    void SetFileLoggingPath(std::string path);

    /// @brief Log a line to the specified source
    /// @param fmt Format string 
    /// Variadic Argument List
    void LogLine(const char* fmt, ...);

    /// @brief Set the source to log lines to
    /// @param output 
    void SetLogOutput(LogOutput output);

    /// @brief Get the current log output source
    LogOutput GetLogOutput();

    /// @brief Returns the current log path
    /// @return The current log path
    std::string GetCurrentLogPath();
}