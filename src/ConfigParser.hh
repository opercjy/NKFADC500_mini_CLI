#ifndef CONFIGPARSER_HH
#define CONFIGPARSER_HH

#include <string>
#include "RunInfo.hh"

class ConfigParser {
public:
    static bool Parse(const std::string& filename, RunInfo* runInfo);
};

#endif
