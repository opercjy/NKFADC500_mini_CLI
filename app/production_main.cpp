#include <iostream>
#include "ELog.hh"

int main(int argc, char** argv) {
    std::cout << "\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m       NKFADC500 Mini - Offline Production\033[0m\n";
    std::cout << "\033[1;36m========================================================\033[0m\n";

    if (argc < 2) {
        ELog::Print(ELog::FATAL, "Usage: ./production_500_mini <raw_data_file.dat>");
        return 1;
    }

    std::string inputFile = argv[1];
    ELog::Print(ELog::INFO, Form("Target Raw File: %s", inputFile.c_str()));
    ELog::Print(ELog::WARNING, "Production module is currently under construction (Phase 3).");

    return 0;
}