#include "ELog.hh"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <mutex>

namespace {
std::mutex gLogMutex;
}

void ELog::Print(Level level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm tm_buf {};
    localtime_r(&now_c, &tm_buf);

    char timeStr[24];
    std::strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &tm_buf);

    const char* levelStr = "[INFO] ";
    const char* colorCode = "\033[1;32m";
    const char* resetCode = "\033[0m";

    switch (level) {
        case INFO:
            levelStr = "[INFO] ";
            colorCode = "\033[1;32m";
            break;
        case WARNING:
            levelStr = "[WARN] ";
            colorCode = "\033[1;33m";
            break;
        case ERROR:
            levelStr = "[ERROR]";
            colorCode = "\033[1;31m";
            break;
        case FATAL:
            levelStr = "[FATAL]";
            colorCode = "\033[1;37;41m";
            break;
    }

    std::lock_guard<std::mutex> lock(gLogMutex);
    std::cout << colorCode << "[" << timeStr << "] " << levelStr << " " << message << resetCode << std::endl;

    if (level == FATAL) {
        std::cout << "\033[1;31mSystem will be terminated due to FATAL error.\033[0m" << std::endl;
        std::exit(EXIT_FAILURE);
    }
}

void ELog::Print(Level level, const char* message) {
    Print(level, std::string(message ? message : ""));
}

void ELog::Print(Level level, const TString& message) {
    Print(level, std::string(message.Data()));
}
