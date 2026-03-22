#include "ELog.hh"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>

void ELog::Print(Level level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    std::time_t now_c = std::chrono::system_clock::to_time_t(now);
    struct tm* parts = std::localtime(&now_c);
    
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", parts);

    std::string levelStr;
    std::string colorCode;
    std::string resetCode = "\033[0m";

    switch (level) {
        case INFO:
            levelStr = "[INFO] ";
            colorCode = "\033[1;32m"; // Bold Green
            break;
        case WARNING:
            levelStr = "[WARN] ";
            colorCode = "\033[1;33m"; // Bold Yellow
            break;
        case ERROR:
            levelStr = "[ERROR]";
            colorCode = "\033[1;31m"; // Bold Red
            break;
        case FATAL:
            levelStr = "[FATAL]";
            colorCode = "\033[1;37;41m"; // White text on Red background
            break;
    }

    std::cout << colorCode << "[" << timeStr << "] " << levelStr << " " << message << resetCode << std::endl;

    if (level == FATAL) {
        std::cout << "\033[1;31m" << "System will be terminated due to FATAL error." << "\033[0m" << std::endl;
        exit(EXIT_FAILURE); 
    }
}

void ELog::Print(Level level, const char* message) {
    Print(level, std::string(message));
}

void ELog::Print(Level level, const TString& message) {
    Print(level, std::string(message.Data()));
}
