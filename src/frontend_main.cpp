#include <iostream>
#include <csignal>
#include <unistd.h>
#include <thread>
#include <chrono>
#include <cstdlib>

#include "BinaryDaqManager.hh"
#include "ConfigParser.hh"
#include "RunInfo.hh"
#include "ELog.hh"

BinaryDaqManager* gDaqManager = nullptr;

void SigIntHandler(int /*signum*/) {
    std::cout << std::endl;
    ELog::Print(ELog::WARNING, "Interrupt signal (Ctrl+C) received! Shutting down gracefully...");
    if (gDaqManager) {
        gDaqManager->Stop();
    }
}

void PrintUsage() {
    std::cout << "\n========================================================\n";
    std::cout << "Usage: frontend_500_mini [options] [config_file]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -f <file>   : Configuration file (Default: ../config/settings.cfg)\n";
    std::cout << "  -o <file>   : Output raw data file (Default: run_XXXX.dat)\n";
    std::cout << "  -n <events> : Number of events to acquire (Default: 0 = Infinite)\n";
    std::cout << "  -t <seconds>: Time limit in seconds (Default: 0 = Infinite)\n";
    std::cout << "  -h          : Print this help message\n";
    std::cout << "========================================================\n\n";
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SigIntHandler);
    std::signal(SIGTERM, SigIntHandler);

    std::string configFile = "../config/settings.cfg";
    std::string outFile = ""; 
    int maxEvents = 0;
    int maxTime = 0;

    int opt;
    // 💡 -n 옵션 추가
    while((opt = getopt(argc, argv, "f:o:n:t:h")) != -1) {
        switch(opt) {
            case 'f': configFile = optarg; break;
            case 'o': outFile = optarg; break;
            case 'n': maxEvents = std::stoi(optarg); break;
            case 't': maxTime = std::stoi(optarg); break;
            case 'h': PrintUsage(); return 0;
            default: PrintUsage(); return 1;
        }
    }

    if (optind < argc) {
        configFile = argv[optind];
    }

    RunInfo runInfo;
    if (!ConfigParser::Parse(configFile, &runInfo)) {
        ELog::Print(ELog::FATAL, "Failed to parse configuration file. Exiting.");
        return 1;
    }
    runInfo.PrintInfo();

    if (outFile.empty()) {
        outFile = Form("run_%04d.dat", runInfo.GetRunNumber());
    }

    // Config 백업
    std::string backupConfig = Form("run_%04d.cfg", runInfo.GetRunNumber());
    std::string copyCmd = "cp " + configFile + " " + backupConfig;
    if (system(copyCmd.c_str()) == 0) {
        ELog::Print(ELog::INFO, Form("Configuration backed up to: %s", backupConfig.c_str()));
    }

    gDaqManager = new BinaryDaqManager(&runInfo);

    std::cout << "\n\033[1;36m========================================================\033[0m" << std::endl;
    std::cout << "\033[1;32m       NKFADC500 Mini Binary DAQ is RUNNING\033[0m" << std::endl;
    std::cout << Form("\033[1;33m       [Target File] %s\033[0m", outFile.c_str()) << std::endl;
    if (maxEvents > 0) std::cout << Form("\033[1;33m       [Limit] %d Events\033[0m", maxEvents) << std::endl;
    if (maxTime > 0)   std::cout << Form("\033[1;33m       [Limit] %d Seconds\033[0m", maxTime) << std::endl;
    std::cout << "\033[1;36m========================================================\033[0m\n" << std::endl;

    // 💡 매니저에 이벤트 제한 개수 전달
    gDaqManager->Start(outFile, maxEvents, maxTime);

    while (gDaqManager->IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    delete gDaqManager;
    ELog::Print(ELog::INFO, "DAQ System fully stopped and safely exited.");

    return 0;
}