#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <iomanip>

#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include "ELog.hh"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "\033[1;36m========================================================\033[0m\n";
        std::cout << "\033[1;32m       NKFADC500 Mini - Offline Production\033[0m\n";
        std::cout << "\033[1;36m========================================================\033[0m\n";
        ELog::Print(ELog::FATAL, "Usage: ./production_500_mini <raw_data_file.dat> [-w]");
        return 1;
    }

    std::string inputFile = "";
    bool saveWaveform = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-w") saveWaveform = true;
        else inputFile = arg;
    }

    if (inputFile.empty()) {
        ELog::Print(ELog::FATAL, "Input file is missing.");
        return 1;
    }

    std::string outputFile = inputFile;
    size_t dotPos = outputFile.find_last_of(".");
    if (dotPos != std::string::npos) {
        outputFile = outputFile.substr(0, dotPos);
    }
    outputFile += "_prod.root"; 

    FILE* fp = fopen(inputFile.c_str(), "rb");
    if (!fp) {
        ELog::Print(ELog::FATAL, Form("Cannot open file: %s", inputFile.c_str()));
        return 1;
    }

    // 💡 [추가] 파일 전체 크기 확인 (진행률 및 ETA 계산용)
    fseek(fp, 0, SEEK_END);
    size_t totalBytes = ftell(fp);
    rewind(fp);
    double totalMB = totalBytes / 1048576.0;

    std::string modeStr = saveWaveform ? "\033[1;35mFull Waveform Mode (-w)\033[0m" : "\033[1;33mFast Physics Mode (Charge/Peak only)\033[0m";

    std::cout << "\n\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m       NKFADC500 Mini - Offline Production\033[0m\n";
    std::cout << "       [Input File]   " << inputFile << " (" << std::fixed << std::setprecision(2) << totalMB << " MB)\n";
    std::cout << "       [Output File]  " << outputFile << "\n";
    std::cout << "       [Process Mode] " << modeStr << "\n";
    std::cout << "\033[1;36m========================================================\033[0m\n\n";

    TFile* rootFile = new TFile(outputFile.c_str(), "RECREATE");
    TTree* tree = new TTree("PROD", "FADC500 Mini Processed Data Tree");
    
    unsigned int eventID = 0;
    unsigned long long triggerTime;
    int runNumber;
    int recordLength;
    
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

        unsigned int data_length = header[0];
        data_length += (header[4] << 8);
        data_length += (header[8] << 16);
        data_length += (header[12] << 24);

        if (data_length <= 32) break; 

        runNumber = header[16] + (header[20] << 8);
        triggerTime = header[44] * 8;
        triggerTime += header[48] * 1000;          
        triggerTime += (header[52] << 8) * 1000;   
        triggerTime += (header[56] << 16) * 1000;  

        int num_samples = (data_length - 32) / 2;
        int payload_bytes = num_samples * 8; 
        recordLength = num_samples;

        std::vector<unsigned char> payload(payload_bytes);
        if (fread(payload.data(), 1, payload_bytes, fp) != (size_t)payload_bytes) {
            break; 
        }
        currentBytes += payload_bytes;

        for(int i=0; i<4; i++) {
            baseline[i] = 0; amplitude[i] = -9999; charge[i] = 0;
            wTime[i].clear(); wDrop[i].clear();
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
            for (int pt = 0; pt < nPed; pt++) {
                pedSum += rawWave[ch][pt];
            }
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
        
        // 💡 [추가] 0.5초 주기로 실시간 대시보드 업데이트
        auto now = std::chrono::steady_clock::now();
        double ui_elapsed = std::chrono::duration<double>(now - ui_timer).count();
        if (ui_elapsed >= 0.5) {
            double total_elapsed = std::chrono::duration<double>(now - start_time).count();
            double progress = (currentBytes / (double)totalBytes) * 100.0;
            double speed_mbps = (currentBytes / 1048576.0) / total_elapsed;
            double rate_hz = eventID / total_elapsed;
            
            double eta_sec = 0;
            if (speed_mbps > 0) {
                eta_sec = (totalMB - (currentBytes / 1048576.0)) / speed_mbps;
            }

            std::cout << "\r\033[F\033[K" << "\033[1;36m[  Production Real-time Monitor  ]\033[0m"
                      << "  ( Elapsed: \033[1;32m" << std::fixed << std::setprecision(1) << total_elapsed << " s\033[0m"
                      << " | ETA: \033[1;33m" << std::fixed << std::setprecision(1) << eta_sec << " s\033[0m )\n"
                      << "\r\033[K" 
                      << "   \033[1;33mProgress:\033[0m " << std::setw(5) << std::fixed << std::setprecision(1) << progress << " % | "
                      << "\033[1;34mEvents:\033[0m " << std::setw(7) << eventID << " | "
                      << "\033[1;35mSpeed:\033[0m " << std::setw(6) << std::fixed << std::setprecision(2) << speed_mbps << " MB/s | "
                      << "\033[1;32mRate:\033[0m " << std::setw(6) << std::fixed << std::setprecision(0) << rate_hz << " Hz"
                      << std::flush;
            
            ui_timer = now;
        }
    }
    
    // 최종 Summary 출력
    auto end_time = std::chrono::steady_clock::now();
    double final_elapsed = std::chrono::duration<double>(end_time - start_time).count();
    double final_speed = totalMB / final_elapsed;
    double final_rate = eventID / final_elapsed;

    std::cout << "\n\n\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m   [ Production Summary ]\033[0m\n";
    std::cout << "   Total Events  : " << eventID << "\n";
    std::cout << "   Time Taken    : " << std::fixed << std::setprecision(2) << final_elapsed << " sec\n";
    std::cout << "   Avg Speed     : " << std::fixed << std::setprecision(2) << final_speed << " MB/s\n";
    std::cout << "   Avg Rate      : " << std::fixed << std::setprecision(0) << final_rate << " Hz\n";
    std::cout << "\033[1;36m========================================================\033[0m\n";
    
    rootFile->Write();
    rootFile->Close();
    fclose(fp);

    ELog::Print(ELog::INFO, Form("Successfully generated: %s", outputFile.c_str()));

    return 0;
}