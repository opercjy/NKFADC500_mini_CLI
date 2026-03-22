#ifndef ELOG_HH
#define ELOG_HH

#include <string>
#include "TString.h"

class ELog {
public:
    enum Level {
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    static void Print(Level level, const std::string& message);
    static void Print(Level level, const char* message);
    static void Print(Level level, const TString& message);
};

#endif
