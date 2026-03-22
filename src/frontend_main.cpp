#include <iostream>
#include <csignal>
#include <unistd.h>
#include <thread>
#include <chrono>

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

int main(int argc, char** argv) {
    std::signal(SIGINT, SigIntHandler);
    std::signal(SIGTERM, SigIntHandler);

    std::string configFile = "../config/settings.cfg";
    std::string outFile = ""; 

    int opt;
    while((opt = getopt(argc, argv, "f:o:")) != -1) {
        switch(opt) {
            case 'f': configFile = optarg; break;
            case 'o': outFile = optarg; break;
        }
    }

    // [핵심] ConfigParser를 이용해 settings.cfg를 읽고 RunInfo 객체에 세팅
    RunInfo runInfo;
    if (!ConfigParser::Parse(configFile, &runInfo)) {
        ELog::Print(ELog::FATAL, "Failed to parse configuration file. Exiting.");
        return 1;
    }
    runInfo.PrintInfo();

    if (outFile.empty()) {
        outFile = Form("run_%04d.dat", runInfo.GetRunNumber());
    }

    // 세팅된 runInfo를 매니저에 넘김 -> 내부에서 Fadc500Device가 이를 읽고 초기화함
    gDaqManager = new BinaryDaqManager(&runInfo);

    std::cout << "\n\033[1;36m========================================================\033[0m" << std::endl;
    std::cout << "\033[1;32m       NKFADC500 Mini Binary DAQ is RUNNING\033[0m" << std::endl;
    std::cout << Form("\033[1;33m       [Target File] %s\033[0m", outFile.c_str()) << std::endl;
    std::cout << "\033[1;36m========================================================\033[0m\n" << std::endl;

    gDaqManager->Start(outFile);

    while (gDaqManager->IsRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    delete gDaqManager;
    ELog::Print(ELog::INFO, "DAQ System fully stopped and safely exited.");

    return 0;
}
