#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>
#include <getopt.h>
#include <cstdlib>
#include <csignal>

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

// 💡 [UX 강화] 직관적이고 아름다운 Usage 출력 함수
void PrintUsage() {
    std::cout << "\n\033[1;36m======================================================================\033[0m\n";
    std::cout << "\033[1;32m      NKFADC500 Mini - High-Speed Data Acquisition Frontend\033[0m\n";
    std::cout << "\033[1;36m======================================================================\033[0m\n";
    std::cout << "\033[1;33mUsage:\033[0m ./frontend_500_mini [options]\n\n";
    std::cout << "\033[1;37m[Optional]\033[0m\n";
    std::cout << "  -f <config>   : Path to settings.cfg (default: ../config/settings.cfg)\n";
    std::cout << "  -o <file>     : Output raw data file (default: test_noise.dat)\n";
    std::cout << "  -n <events>   : Stop after N events (default: 0 = infinite)\n";
    std::cout << "  -t <sec>      : Stop after T seconds (default: 0 = infinite)\n";
    std::cout << "  -h            : Print this help message\n";
    std::cout << "\033[1;36m======================================================================\033[0m\n\n";
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

    // 💡 인자 없이 실행 시 자동으로 도움말 출력
    if (argc == 1) {
        PrintUsage();
        return 1;
    }

    // 시그널 핸들러 등록
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

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

    // DAQ 매니저 생성 및 가동
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