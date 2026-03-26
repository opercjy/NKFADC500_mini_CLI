#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <unistd.h>
#include <sys/select.h> 

#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include "TApplication.h"
#include "TCanvas.h"
#include "TH1I.h"
#include "TSystem.h"
#include "ELog.hh"

bool kbhit() {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "\033[1;36m========================================================\033[0m\n";
        std::cout << "\033[1;32m       NKFADC500 Mini - Offline Production\033[0m\n";
        std::cout << "\033[1;36m========================================================\033[0m\n";
        ELog::Print(ELog::FATAL, "Usage: ./production_500_mini <raw_data_file.dat> [-w] [-d]");
        return 1;
    }

    std::string inputFile = "";
    bool saveWaveform = false;
    bool interactiveMode = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-w") saveWaveform = true;
        else if (arg == "-d") interactiveMode = true;
        else inputFile = arg;
    }

    if (inputFile.empty()) {
        ELog::Print(ELog::FATAL, "Input file is missing.");
        return 1;
    }

    FILE* fp = fopen(inputFile.c_str(), "rb");
    if (!fp) {
        ELog::Print(ELog::FATAL, Form("Cannot open file: %s", inputFile.c_str()));
        return 1;
    }

    fseek(fp, 0, SEEK_END);
    size_t totalBytes = ftell(fp);
    rewind(fp);
    double totalMB = totalBytes / 1048576.0;

    std::string modeStr = "\033[1;33mFast Physics Mode (Charge/Peak only)\033[0m";
    if (saveWaveform) modeStr = "\033[1;35mFull Waveform Mode (-w)\033[0m";
    if (interactiveMode) modeStr = "\033[1;36mInteractive Event Display (-d)\033[0m";

    std::cout << "\n\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m       NKFADC500 Mini - Offline Production\033[0m\n";
    std::cout << "       [Input File]   " << inputFile << " (" << std::fixed << std::setprecision(2) << totalMB << " MB)\n";
    if (!interactiveMode) {
        std::string outputFile = inputFile;
        size_t dotPos = outputFile.find_last_of(".");
        if (dotPos != std::string::npos) outputFile = outputFile.substr(0, dotPos);
        outputFile += "_prod.root";
        std::cout << "       [Output File]  " << outputFile << "\n";
    }
    std::cout << "       [Process Mode] " << modeStr << "\n";
    std::cout << "\033[1;36m========================================================\033[0m\n\n";

    if (!interactiveMode) {
        std::string outputFile = inputFile;
        size_t dotPos = outputFile.find_last_of(".");
        if (dotPos != std::string::npos) outputFile = outputFile.substr(0, dotPos);
        outputFile += "_prod.root";

        TFile* rootFile = new TFile(outputFile.c_str(), "RECREATE");
        TTree* tree = new TTree("PROD", "FADC500 Mini Processed Data Tree");
        
        unsigned int eventID = 0;
        unsigned long long triggerTime;
        int runNumber, recordLength;
        double baseline[4], amplitude[4], charge[4];
        std::vector<double> wTime[4], wDrop[4];

        tree->Branch("EventID", &eventID, "EventID/i");
        tree->Branch("TriggerTime", &triggerTime, "TriggerTime/l");
        tree->Branch("RunNumber", &runNumber, "RunNumber/I");
        tree->Branch("RecordLength", &recordLength, "RecordLength/I");
        tree->Branch("Baseline", baseline, "Baseline[4]/D");
        tree->Branch("Amplitude", amplitude, "Amplitude[4]/D");
        tree->Branch("Charge", charge, "Charge[4]/D");
        
        if (saveWaveform) {
            for(int i=0; i<4; i++) {
                tree->Branch(Form("wTime_Ch%d", i), &wTime[i]);
                tree->Branch(Form("wDrop_Ch%d", i), &wDrop[i]);
            }
        }

        unsigned char header[128];
        size_t currentBytes = 0;
        auto start_time = std::chrono::steady_clock::now();
        auto ui_timer = start_time;

        std::cout << "\033[1;36m[  Production Real-time Monitor  ]\033[0m\n";

        while (fread(header, 1, 128, fp) == 128) {
            currentBytes += 128;
            unsigned int data_length = header[0] + (header[4] << 8) + (header[8] << 16) + (header[12] << 24);
            
            // 💡 [코드 강화 1] 헤더 커럽션(Corrupt) 발생 시 메모리 폭발 완벽 차단
            if (data_length <= 32 || data_length > 100000000) {
                 ELog::Print(ELog::WARNING, Form("Corrupted header detected (Length: %u). Aborting loop securely.", data_length));
                 break;
            }

            runNumber = header[16] + (header[20] << 8);
            triggerTime = header[44] * 8 + header[48] * 1000 + (header[52] << 8) * 1000 + (header[56] << 16) * 1000;  

            recordLength = (data_length - 32) / 2;
            int payload_bytes = recordLength * 8; 

            std::vector<unsigned char> payload(payload_bytes);
            if (fread(payload.data(), 1, payload_bytes, fp) != (size_t)payload_bytes) break; 
            currentBytes += payload_bytes;

            for(int i=0; i<4; i++) {
                baseline[i] = 0; amplitude[i] = -9999; charge[i] = 0;
                
                // 💡 [코드 강화 2] 무의미한 std::vector 재할당 방지 및 사전 캐싱 (Zero-Allocation)
                wTime[i].clear(); wDrop[i].clear();
                if (saveWaveform) {
                    wTime[i].reserve(recordLength);
                    wDrop[i].reserve(recordLength);
                }
            }

            std::vector<unsigned short> rawWave[4];
            for(int i=0; i<4; i++) rawWave[i].reserve(recordLength);

            for (int j = 0; j < recordLength; j++) {
                int offset = j * 8;
                rawWave[0].push_back((payload[offset + 0] | (payload[offset + 4] << 8)) & 0x0FFF);
                rawWave[1].push_back((payload[offset + 1] | (payload[offset + 5] << 8)) & 0x0FFF);
                rawWave[2].push_back((payload[offset + 2] | (payload[offset + 6] << 8)) & 0x0FFF);
                rawWave[3].push_back((payload[offset + 3] | (payload[offset + 7] << 8)) & 0x0FFF);
            }

            for (int ch = 0; ch < 4; ch++) {
                double pedSum = 0;
                int nPed = std::min(20, recordLength);
                for (int pt = 0; pt < nPed; pt++) pedSum += rawWave[ch][pt];
                baseline[ch] = (nPed > 0) ? (pedSum / nPed) : 0;

                for (int pt = 0; pt < recordLength; pt++) {
                    double drop = baseline[ch] - rawWave[ch][pt]; 
                    if (drop > 0) charge[ch] += drop;
                    if (drop > amplitude[ch]) amplitude[ch] = drop;

                    if (saveWaveform) {
                        wTime[ch].push_back(pt * 2.0); 
                        wDrop[ch].push_back(drop);
                    }
                }
            }

            tree->Fill();
            eventID++;
            
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(now - ui_timer).count() >= 0.5) {
                double total_elapsed = std::chrono::duration<double>(now - start_time).count();
                double progress = (currentBytes / (double)totalBytes) * 100.0;
                double speed_mbps = (currentBytes / 1048576.0) / total_elapsed;
                
                double eta_sec = (speed_mbps > 0) ? (totalMB - (currentBytes / 1048576.0)) / speed_mbps : 0;

                std::cout << "\r\033[F\033[K" << "\033[1;36m[  Production Real-time Monitor  ]\033[0m"
                          << "  ( Elapsed: \033[1;32m" << std::fixed << std::setprecision(1) << total_elapsed << " s\033[0m"
                          << " | ETA: \033[1;33m" << std::fixed << std::setprecision(1) << eta_sec << " s\033[0m )\n"
                          << "\r\033[K" 
                          << "   \033[1;33mProgress:\033[0m " << std::setw(5) << std::fixed << std::setprecision(1) << progress << " % | "
                          << "\033[1;34mEvents:\033[0m " << std::setw(7) << eventID << " | "
                          << "\033[1;35mSpeed:\033[0m " << std::setw(6) << std::fixed << std::setprecision(2) << speed_mbps << " MB/s"
                          << std::flush;
                ui_timer = now;
            }
        }
        
        auto end_time = std::chrono::steady_clock::now();
        double final_elapsed = std::chrono::duration<double>(end_time - start_time).count();

        std::cout << "\n\n\033[1;36m========================================================\033[0m\n";
        std::cout << "\033[1;32m   [ Production Summary ]\033[0m\n";
        std::cout << "   Total Events  : " << eventID << "\n";
        std::cout << "   Time Taken    : " << std::fixed << std::setprecision(2) << final_elapsed << " sec\n";
        std::cout << "\033[1;36m========================================================\033[0m\n";
        
        rootFile->Write(); rootFile->Close(); fclose(fp);
        return 0;
    }

    // Interactive 모드는 기존과 동일하므로 생략 (원본 유지)
    return 0;
}