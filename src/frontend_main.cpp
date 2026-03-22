#include <iostream>
#include <csignal>
#include <atomic>
#include <unistd.h>

#include "BinaryDaqManager.hh"
#include "RunInfo.hh"
#include "ELog.hh"

// 전역 객체로 선언하여 시그널 핸들러에서 접근 가능하도록 함
BinaryDaqManager* gDaqManager = nullptr;

void SigIntHandler(int /*signum*/) {
    std::cout << "\n\033[1;33m[INFO] Interrupt signal (Ctrl+C) received! Shutting down gracefully...\033[0m" << std::endl;
    if (gDaqManager) {
        gDaqManager->Stop();
    }
}

int main(int argc, char** argv) {
    std::signal(SIGINT, SigIntHandler);
    std::signal(SIGTERM, SigIntHandler);

    std::string configFile = "config/settings.cfg";
    std::string outFile = "raw_data.dat"; // 순수 바이너리 확장자

    int opt;
    while((opt = getopt(argc, argv, "f:o:")) != -1) {
        switch(opt) {
            case 'f': configFile = optarg; break;
            case 'o': outFile = optarg; break;
        }
    }

    // 1. RunInfo 객체 생성 및 (TODO: Config 파싱 로직 적용)
    RunInfo runInfo;
    FadcBD* bd = runInfo.AddFadcBD(0); // MID = 0 으로 보드 1개 세팅
    bd->SetTHR(0, 50); // 기본 임시 세팅값 (향후 ConfigParser 연동)
    bd->SetDACOFF(0, 2048);

    runInfo.PrintInfo();

    // 2. DAQ 관리자 초기화
    gDaqManager = new BinaryDaqManager(&runInfo);

    std::cout << "\n\033[1;32m====================================================\033[0m" << std::endl;
    std::cout << "\033[1;32m      NKFADC500 Mini Binary DAQ is RUNNING\033[0m" << std::endl;
    std::cout << "\033[1;32m      (Press Ctrl+C to abort and save data)\033[0m" << std::endl;
    std::cout << "\033[1;32m====================================================\033[0m\n" << std::endl;

    // 3. 수집 시작
    gDaqManager->Start(outFile);

    // 4. 메인 스레드 유지
    while (gDaqManager->IsRunning()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    delete gDaqManager;
    std::cout << "\033[1;36m[DAQ] System fully stopped and safely exited.\033[0m" << std::endl;

    return 0;
}
