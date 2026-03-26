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
    // 💡 [버그 픽스] Interactive 모드 (-d) 위아래(Positive-going) 반전 렌더링 적용
    // =================================================================================
    if (interactiveMode) {
        TApplication app("app", &argc, argv);
        TCanvas* c1 = new TCanvas("c1", "NKFADC500 Mini - Interactive Event Display", 1400, 900);
        c1->Divide(2, 2);

        TH1I* hWave[4];
        TGraph* gFill[4]; 

        for (int i = 0; i < 4; i++) {
            // Y축 라벨을 순수 신호 크기(Signal Amplitude)로 변경
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

        std::cout << "\n\033[1;35m========================================================\033[0m\n";
        std::cout << "\033[1;32m   [ Interactive Display Mode Activated ]\033[0m\n";
        std::cout << "   -> GUI 창이나 터미널에서 \033[1;33m[ENTER]\033[0m 키를 누르면 다음 이벤트로 넘어갑니다.\n";
        std::cout << "   -> 종료하려면 \033[1;31m'q'\033[0m 입력 후 엔터를 치세요.\n";
        std::cout << "\033[1;35m========================================================\033[0m\n\n";

        while (fread(header, 1, 128, fp) == 128) {
            unsigned int data_length = header[0] + (header[4] << 8) + (header[8] << 16) + (header[12] << 24);
            
            if (data_length <= 32 || data_length > 100000000) {
                 ELog::Print(ELog::WARNING, Form("Corrupted header detected. Aborting."));
                 break;
            }

            int recordLength = (data_length - 32) / 2;
            int payload_bytes = recordLength * 8; 

            std::vector<unsigned char> payload(payload_bytes);
            if (fread(payload.data(), 1, payload_bytes, fp) != (size_t)payload_bytes) break; 

            // 1. 먼저 파일에서 꺼낸 Raw 파형을 임시 배열에 저장 (페데스탈을 먼저 구해야 반전을 시킬 수 있으므로)
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
                
                // 2. 동적 페데스탈 계산 (앞 20개 샘플 평균)
                double pedSum = 0;
                int nPed = std::min(20, recordLength);
                for (int pt = 0; pt < nPed; pt++) {
                    pedSum += rawWave[i][pt];
                }
                double baseline = (nPed > 0) ? (pedSum / nPed) : 0;

                // 💡 [핵심] 3. 페데스탈(0)을 기준으로 위아래 반전(Invert) 적용 및 폴리곤 채우기
                gFill[i]->Set(0); 
                gFill[i]->SetPoint(0, 0, 0); // 반전되었으므로 기준선은 완벽한 0 입니다.
                
                for (int j = 0; j < recordLength; j++) {
                    // (기준선 - Raw) 연산을 통해 신호가 위로 솟구치도록(Positive-going) 변환!
                    double inverted_sig = baseline - rawWave[i][j]; 
                    
                    hWave[i]->SetBinContent(j + 1, inverted_sig); // 뼈대 업데이트
                    gFill[i]->SetPoint(j + 1, j * 2.0, inverted_sig); // 빗금 칠할 다각형 꼭짓점 추가
                }
                gFill[i]->SetPoint(recordLength + 1, (recordLength - 1) * 2.0, 0); // 폴리곤 끝점 바닥(0) 마감

                hWave[i]->SetTitle(Form("Event %u - Channel %d (Base: %.1f);Time (ns);Signal Amplitude (ADC)", eventID, i, baseline));
                
                // Y축은 반전되었으므로 -100 (노이즈 흔들림용) ~ 4100 (12-bit 최대치) 영역으로 고정
                hWave[i]->GetYaxis()->SetRangeUser(-100, 4200); 
                
                hWave[i]->Draw("HIST"); 
                gFill[i]->Draw("F SAME"); 

                // 💡 [핵심] 4. 베이스라인(이제 0)의 궤적을 알려주는 빨간 점선 가이드
                TLine lPed;
                lPed.SetLineColor(kRed);
                lPed.SetLineStyle(2);
                lPed.DrawLine(0, 0, (recordLength - 1) * 2.0, 0);
            }
            c1->Modified();
            c1->Update();

            std::cout << "\033[1;36mEvent " << eventID << " Loaded.\033[0m Press ENTER for next, 'q' to quit : ";
            std::string input;
            std::getline(std::cin, input);
            if (input == "q" || input == "Q") {
                std::cout << "\033[1;33mUser requested exit.\033[0m\n";
                break;
            }

            eventID++;
        }

        std::cout << "\n\033[1;32m[ Interactive Display Terminated ]\033[0m\n";
        fclose(fp);
        return 0;
    }

    return 0;
}
