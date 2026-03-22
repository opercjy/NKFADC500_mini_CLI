#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <getopt.h>
#include <cstdlib>

#include "RunInfo.hh"
#include "ConfigParser.hh"
#include "BinaryDaqManager.hh"
#include "ELog.hh"
#include "TString.h"

BinaryDaqManager* gDaqManager = nullptr;

// Ctrl+C 인터럽트 처리기 (안전 종료)
void SignalHandler(int signum) {
    std::cout << "\n";
    ELog::Print(ELog::WARNING, "Interrupt signal (Ctrl+C) received! Shutting down gracefully...");
    if (gDaqManager) {
        gDaqManager->Stop();
    }
}

// 도움말 출력
void PrintUsage() {
    std::cout << "Usage: ./frontend_500_mini [options]\n"
              << "Options:\n"
              << "  -f <config_file>   : Path to settings.cfg (default: ../config/settings.cfg)\n"
              << "  -o <output_file>   : Output raw data file (default: test_noise.dat)\n"
              << "  -n <max_events>    : Stop after N events (default: 0 = infinite)\n"
              << "  -t <max_time_sec>  : Stop after T seconds (default: 0 = infinite)\n"
              << "  -h                 : Print this help message\n";
}

int main(int argc, char** argv) {
    std::string configFile = "../config/settings.cfg";
    std::string outFile = "test_noise.dat";
    int maxEvents = 0;
    int maxTime = 0;

    // 명령줄 인수 파싱
    int opt;
    while ((opt = getopt(argc, argv, "f:o:n:t:h")) != -1) {
        switch (opt) {
            case 'f': configFile = optarg; break;
            case 'o': outFile = optarg; break;
            case 'n': maxEvents = std::atoi(optarg); break;
            case 't': maxTime = std::atoi(optarg); break;
            case 'h': PrintUsage(); return 0;
            default: PrintUsage(); return 1;
        }
    }

    // 시그널 핸들러 등록
    std::signal(SIGINT, SignalHandler);

    // 설정 파싱
    RunInfo runInfo;
    ConfigParser parser;
    if (!parser.Parse(configFile, &runInfo)) {
        return 1;
    }

    // Config 백업
    std::string backupConfig = Form("run_%04d.cfg", runInfo.GetRunNumber());
    std::string copyCmd = "cp " + configFile + " " + backupConfig;
    if (system(copyCmd.c_str()) == 0) {
        ELog::Print(ELog::INFO, Form("Configuration backed up to: %s", backupConfig.c_str()));
    }

    // DAQ 매니저 생성 및 가동 (배너는 내부에서 출력됨)
    gDaqManager = new BinaryDaqManager(&runInfo);
    gDaqManager->Start(outFile, maxEvents, maxTime);

    // 메인 스레드는 DAQ가 끝날 때까지 대기
    while (gDaqManager->IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    // 안전하게 자원 해제
    delete gDaqManager;
    
    ELog::Print(ELog::INFO, "DAQ System fully stopped and safely exited.");

    return 0;
}