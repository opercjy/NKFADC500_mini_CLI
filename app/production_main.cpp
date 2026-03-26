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
#include "TGraph.h"
#include "TLine.h"
#include "TSystem.h"
#include "ELog.hh"

// 비동기 키보드 입력 감지 (ROOT 캔버스 렌더링 안 멈추게 하기 위함)
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

    // =================================================================================
    // 💡 [개선] Interactive 모드 (-d) - 지능형 이벤트 점프(Jump) 기능 탑재
    // =================================================================================
    if (interactiveMode) {
        TApplication app("app", &argc, argv);
        TCanvas* c1 = new TCanvas("c1", "NKFADC500 Mini - Interactive Event Display", 1400, 900);
        c1->Divide(2, 2);

        TH1I* hWave[4];
        TGraph* gFill[4]; 

        for (int i = 0; i < 4; i++) {
            hWave[i] = new TH1I(Form("hWave_Ch%d", i), Form("Channel %d Signal;Time (ns);Signal Amplitude (ADC)", i), 1000, 0, 2000);
            hWave[i]->SetStats(0);
            hWave[i]->SetLineColor(kAzure + 2);
            hWave[i]->SetLineWidth(2);
            hWave[i]->SetFillStyle(0); 

            gFill[i] = new TGraph();
            gFill[i]->SetFillColor(kAzure - 9);
            gFill[i]->SetFillStyle(3004); 
        }

        unsigned char header[128];
        unsigned int eventID = 0;
        unsigned int targetEventID = 0; // 💡 점프 목표 이벤트

        std::cout << "\n\033[1;35m========================================================\033[0m\n";
        std::cout << "\033[1;32m   [ Interactive Display Mode Activated ]\033[0m\n";
        std::cout << "   -> \033[1;33m[ENTER]\033[0m   : Next Event (다음 이벤트로 이동)\n";
        std::cout << "   -> \033[1;36m[NUMBER]\033[0m  : Jump to Event (해당 이벤트 번호로 워프)\n";
        std::cout << "   -> \033[1;31m[q]\033[0m       : Quit (종료)\n";
        std::cout << "\033[1;35m========================================================\033[0m\n\n";

        while (fread(header, 1, 128, fp) == 128) {
            unsigned int data_length = header[0] + (header[4] << 8) + (header[8] << 16) + (header[12] << 24);
            
            if (data_length <= 32 || data_length > 100000000) {
                 ELog::Print(ELog::WARNING, Form("Corrupted header detected. Aborting."));
                 break;
            }

            int recordLength = (data_length - 32) / 2;
            int payload_bytes = recordLength * 8; 

            // 💡 [점프 엔진] 목표 이벤트 번호에 도달하지 않았다면, payload를 읽지 않고 fseek로 초고속 스킵!
            if (eventID < targetEventID) {
                fseek(fp, payload_bytes, SEEK_CUR);
                eventID++;
                continue;
            }

            std::vector<unsigned char> payload(payload_bytes);
            if (fread(payload.data(), 1, payload_bytes, fp) != (size_t)payload_bytes) break; 

            std::vector<std::vector<unsigned short>> rawWave(4, std::vector<unsigned short>(recordLength));

            // 12-bit 디마스킹
            for (int j = 0; j < recordLength; j++) {
                int offset = j * 8;
                rawWave[0][j] = (payload[offset + 0] | (payload[offset + 4] << 8)) & 0x0FFF;
                rawWave[1][j] = (payload[offset + 1] | (payload[offset + 5] << 8)) & 0x0FFF;
                rawWave[2][j] = (payload[offset + 2] | (payload[offset + 6] << 8)) & 0x0FFF;
                rawWave[3][j] = (payload[offset + 3] | (payload[offset + 7] << 8)) & 0x0FFF;
            }

            for (int i = 0; i < 4; i++) {
                hWave[i]->Reset();
                hWave[i]->SetBins(recordLength, 0, recordLength * 2.0); 
                c1->cd(i + 1);
                gPad->SetGrid();
                
                double pedSum = 0;
                int nPed = std::min(20, recordLength);
                for (int pt = 0; pt < nPed; pt++) pedSum += rawWave[i][pt];
                double baseline = (nPed > 0) ? (pedSum / nPed) : 0;

                gFill[i]->Set(0); 
                gFill[i]->SetPoint(0, 0, 0); 
                
                for (int j = 0; j < recordLength; j++) {
                    double inverted_sig = baseline - rawWave[i][j]; 
                    hWave[i]->SetBinContent(j + 1, inverted_sig); 
                    gFill[i]->SetPoint(j + 1, j * 2.0, inverted_sig); 
                }
                gFill[i]->SetPoint(recordLength + 1, (recordLength - 1) * 2.0, 0); 

                hWave[i]->SetTitle(Form("Event %u - Channel %d (Base: %.1f);Time (ns);Signal Amplitude (ADC)", eventID, i, baseline));
                hWave[i]->GetYaxis()->SetRangeUser(-100, 4200); 
                
                hWave[i]->Draw("HIST"); 
                gFill[i]->Draw("F SAME"); 

                TLine lPed;
                lPed.SetLineColor(kRed);
                lPed.SetLineStyle(2);
                lPed.DrawLine(0, 0, (recordLength - 1) * 2.0, 0);
            }
            c1->Modified();
            c1->Update();

            // 💡 [상호작용 엔진] 사용자가 번호를 입력할 때까지 대기 (이때 ROOT 캔버스는 얼지 않고 작동함!)
            bool requires_rewind = false;
            while (true) {
                std::cout << "\033[1;36mEvent " << eventID << " Loaded.\033[0m [ENTER]: Next | [q]: Quit | [Number]: Jump -> " << std::flush;
                
                // 엔터키가 들어오기 전까지 ROOT 캔버스 GUI 이벤트를 계속 살려둠 (줌, 패닝 등 가능)
                while (!kbhit()) {
                    gSystem->ProcessEvents();
                    usleep(10000);
                }

                std::string input;
                std::getline(std::cin, input);
                
                if (input.empty()) {
                    targetEventID = eventID + 1; // 다음 이벤트
                    break;
                } else if (input == "q" || input == "Q") {
                    std::cout << "\033[1;33mUser requested exit.\033[0m\n";
                    fclose(fp);
                    return 0;
                } else {
                    try {
                        int jump_idx = std::stoi(input);
                        if (jump_idx < 0) {
                            std::cout << "\033[1;31mEvent number cannot be negative.\033[0m\n";
                        } else if (jump_idx <= (int)eventID) {
                            std::cout << "\033[1;33mRewinding to Event " << jump_idx << "...\033[0m\n";
                            rewind(fp); // 💡 타임머신: 과거 이벤트로 돌아가기 위해 파일을 맨 처음으로 되감음
                            eventID = 0;
                            targetEventID = jump_idx;
                            requires_rewind = true;
                            break;
                        } else {
                            std::cout << "\033[1;33mFast-forwarding to Event " << jump_idx << "...\033[0m\n";
                            targetEventID = jump_idx; // 💡 미래 이벤트: 다음 루프부터 그 번호가 될때까지 안 그리고 고속 패스함
                            break;
                        }
                    } catch (...) {
                        std::cout << "\033[1;31mInvalid input. Enter a number or 'q'.\033[0m\n";
                    }
                }
            }

            if (requires_rewind) continue; // 과거로 갔으면 eventID 올리지 않고 파일 처음부터 루프 재시작
            eventID++;
        }

        std::cout << "\n\033[1;32m[ Interactive Display Terminated ]\033[0m\n";
        fclose(fp);
        return 0;
    }

    return 0;
}
