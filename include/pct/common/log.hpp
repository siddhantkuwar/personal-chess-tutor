#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <string_view>

namespace pct {

enum class LogLevel { Debug, Info, Warning, Error };

inline void log(LogLevel level, std::string_view component, std::string_view message) {
    static std::mutex mutex;
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    const char* label = "INFO";
    switch (level) {
    case LogLevel::Debug:
        label = "DEBUG";
        break;
    case LogLevel::Info:
        break;
    case LogLevel::Warning:
        label = "WARN";
        break;
    case LogLevel::Error:
        label = "ERROR";
        break;
    }
    std::lock_guard lock(mutex);
    std::clog << std::put_time(std::localtime(&time), "%Y-%m-%dT%H:%M:%S") << ' ' << label << " ["
              << component << "] " << message << '\n';
}

} // namespace pct
