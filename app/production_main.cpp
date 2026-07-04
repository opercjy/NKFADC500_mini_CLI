#include <iostream>
#include <fstream>
#include <sstream>
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

// =========================================================================
// [아키텍처 확장] Browser History Cache Manager (로컬 파일 DB)
// =========================================================================
class HistoryCache {
private:
    std::string cacheFile = ".fadc_browser.cache";
public:
    std::string GetLastFile() {
        std::ifstream in(cacheFile);
        std::string file = "";
        if (in.is_open()) {
            std::getline(in, file);
            in.close();
        }
        return file;
    }
    void SaveFile(const std::string& file) {
        std::ofstream out(cacheFile);
        if (out.is_open()) {
            out << file;
            out.close();
        }
    }
};

// =========================================================================
// 💡 [수정 완료] Configuration Parser (설정 파일 DLY 파싱)
// =========================================================================
double GetTriggerDelayFromConfig(const std::string& configPath) {
    double delay_ns = 400.0; // Default fallback
    std::ifstream cfg(configPath);
    if (cfg.is_open()) {
        std::string line;
        while (std::getline(cfg, line)) {
            // 주석 및 빈 줄 무시
            if (line.empty() || line[0] == '#') continue; 
            
            std::istringstream iss(line);
            std::string key;
            // 💡 "DLY" 키워드와 정확히 일치하는 경우에만 값을 파싱
            if (iss >> key) {
                if (key == "DLY") {
                    if (iss >> delay_ns) {
                        break;
                    }
                }
            }
        }
        cfg.close();
    } else {
        ELog::Print(ELog::WARNING, "config/settings.cfg not found. Using default DLY = 400.0 ns");
    }
    return delay_ns;
}

// 비동기 키보드 입력 감지
bool kbhit() {
    struct timeval tv = { 0L, 0L };
    fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

void PrintUsage() {
    std::cout << "\n\033[1;36m======================================================================\033[0m\n";
    std::cout << "\033[1;32m      NKFADC500 Mini - Offline Production & Analysis Tool\033[0m\n";
    std::cout << "\033[1;36m======================================================================\033[0m\n";
    std::cout << "\033[1;33mUsage:\033[0m ./production_500_mini [raw_data_file.dat] [options]\n\n";
    std::cout << "\033[1;37m[Auto Cache Load]\033[0m\n";
    std::cout << "  If no file is provided, the tool loads the last used file from cache.\n\n";
    std::cout << "\033[1;37m[Optional]\033[0m\n";
    std::cout << "  -w             : Save full waveforms in the output tree (Warning: Large File)\n";
    std::cout << "  -d             : Interactive Event Display Mode (Visual Waveform Debugger)\n";
    std::cout << "\033[1;36m======================================================================\033[0m\n\n";
}

int main(int argc, char** argv) {
    std::string inputFile = "";
    bool saveWaveform = false;
    bool interactiveMode = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-w") saveWaveform = true;
        else if (arg == "-d") interactiveMode = true;
        else if (arg[0] != '-') inputFile = arg;
    }

    HistoryCache cache;
    if (inputFile.empty()) {
        inputFile = cache.GetLastFile();
        if (inputFile.empty()) {
            PrintUsage();
            return 1;
        } else {
            std::cout << "\033[1;32m[INFO] Auto-loading last used file from cache DB:\033[0m " << inputFile << "\n";
        }
    } else {
        cache.SaveFile(inputFile);
    }
    
    if (interactiveMode && gSystem->Getenv("DISPLAY") == nullptr) {
        std::cout << "\n\033[1;31m[FATAL ERROR]\033[0m No X11 DISPLAY found!\n";
        std::cout << "Cannot open Interactive Canvas in Headless mode.\n";
        std::cout << "Please reconnect via SSH using '\033[1;33mssh -X\033[0m' or '\033[1;33mssh -Y\033[0m'.\n\n";
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

    // 트리거 딜레이(DLY) 파싱 및 동적 베이스라인 윈도우(40%) 계산
    double trigger_delay_ns = GetTriggerDelayFromConfig("config/settings.cfg");
    double base_window_ns = trigger_delay_ns * 0.40;

    std::string modeStr = "\033[1;33mFast Physics Mode (Channel-wise Isolated)\033[0m";
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
    std::cout << "       [Trig. Delay]  " << trigger_delay_ns << " ns (Base. Window: " << base_window_ns << " ns)\n";
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
        double baseline[4], amplitude[4], charge[4], peakTime[4]; 
        std::vector<double> wTime[4], wDrop[4];

        tree->Branch("EventID", &eventID, "EventID/i");
        tree->Branch("TriggerTime", &triggerTime, "TriggerTime/l");
        tree->Branch("RunNumber", &runNumber, "RunNumber/I");
        tree->Branch("RecordLength", &recordLength, "RecordLength/I");

        for(int i=0; i<4; i++) {
            tree->Branch(Form("Baseline_Ch%d", i), &baseline[i], Form("Baseline_Ch%d/D", i));
            tree->Branch(Form("Amplitude_Ch%d", i), &amplitude[i], Form("Amplitude_Ch%d/D", i));
            tree->Branch(Form("Charge_Ch%d", i), &charge[i], Form("Charge_Ch%d/D", i));
            tree->Branch(Form("PeakTime_Ch%d", i), &peakTime[i], Form("PeakTime_Ch%d/D", i));
            
            if (saveWaveform) {
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
                baseline[i] = 0; amplitude[i] = -9999; charge[i] = 0; peakTime[i] = 0;
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

            // 💡 딜레이(DLY) 기반 동적 베이스라인 산출
            int nPed = std::min(static_cast<int>((trigger_delay_ns / 2.0) * 0.40), recordLength);

            for (int ch = 0; ch < 4; ch++) {
                double pedSum = 0;
                for (int pt = 0; pt < nPed; pt++) pedSum += rawWave[ch][pt];
                baseline[ch] = (nPed > 0) ? (pedSum / nPed) : 0;

                int maxIdx = 0; 
                for (int pt = 0; pt < recordLength; pt++) {
                    double drop = baseline[ch] - rawWave[ch][pt]; 
                    if (drop > 0) charge[ch] += drop;
                    
                    if (drop > amplitude[ch]) {
                        amplitude[ch] = drop;
                        maxIdx = pt;
                    }

                    if (saveWaveform) {
                        wTime[ch].push_back(pt * 2.0); 
                        wDrop[ch].push_back(drop);
                    }
                }
                peakTime[ch] = maxIdx * 2.0;
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
    // Interactive 모드 (-d)
    // =================================================================================
    if (interactiveMode) {
        TApplication app("app", &argc, argv);
        TCanvas* c1 = new TCanvas("c1", "NKFADC500 Mini - Interactive Event Display", 1400, 900);
        c1->Divide(2, 2);

        TH1I* hWave[4];
        TGraph* gFill[4]; 
        TLine* lPed[4] = {nullptr, nullptr, nullptr, nullptr}; 

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
        unsigned int targetEventID = 0; 

        std::cout << "\n\033[1;35m========================================================\033[0m\n";
        std::cout << "\033[1;32m   [ Interactive Display Mode Activated ]\033[0m\n";
        std::cout << "   -> \033[1;33m[ENTER]\033[0m   : Next Event\n";
        std::cout << "   -> \033[1;36m[p]\033[0m       : Previous Event\n";
        std::cout << "   -> \033[1;36m[j]\033[0m       : Jump to Event\n";
        std::cout << "   -> \033[1;31m[q]\033[0m       : Quit\n";
        std::cout << "\033[1;35m========================================================\033[0m\n\n";

        while (fread(header, 1, 128, fp) == 128) {
            unsigned int data_length = header[0] + (header[4] << 8) + (header[8] << 16) + (header[12] << 24);
            
            if (data_length <= 32 || data_length > 100000000) {
                 ELog::Print(ELog::WARNING, Form("Corrupted header detected. Aborting."));
                 break;
            }

            int recordLength = (data_length - 32) / 2;
            int payload_bytes = recordLength * 8; 

            if (eventID < targetEventID) {
                fseek(fp, payload_bytes, SEEK_CUR);
                eventID++;
                continue;
            }

            std::vector<unsigned char> payload(payload_bytes);
            if (fread(payload.data(), 1, payload_bytes, fp) != (size_t)payload_bytes) break; 

            std::vector<std::vector<unsigned short>> rawWave(4, std::vector<unsigned short>(recordLength));

            for (int j = 0; j < recordLength; j++) {
                int offset = j * 8;
                rawWave[0][j] = (payload[offset + 0] | (payload[offset + 4] << 8)) & 0x0FFF;
                rawWave[1][j] = (payload[offset + 1] | (payload[offset + 5] << 8)) & 0x0FFF;
                rawWave[2][j] = (payload[offset + 2] | (payload[offset + 6] << 8)) & 0x0FFF;
                rawWave[3][j] = (payload[offset + 3] | (payload[offset + 7] << 8)) & 0x0FFF;
            }

            std::cout << "\n\033[1;36m=== Event " << eventID << " ===\033[0m\n";

            int nPed = std::min(static_cast<int>((trigger_delay_ns / 2.0) * 0.40), recordLength);

            for (int i = 0; i < 4; i++) {
                hWave[i]->Reset();
                hWave[i]->SetBins(recordLength, 0, recordLength * 2.0); 
                c1->cd(i + 1);
                gPad->SetGrid();
                
                double pedSum = 0;
                for (int pt = 0; pt < nPed; pt++) pedSum += rawWave[i][pt];
                double baseline = (nPed > 0) ? (pedSum / nPed) : 0;

                gFill[i]->Set(0); 
                gFill[i]->SetPoint(0, 0, 0); 
                
                double maxAmp = -9999.0;
                double qSum = 0.0;
                int maxIdx = 0;
                
                for (int j = 0; j < recordLength; j++) {
                    double inverted_sig = baseline - rawWave[i][j]; 
                    hWave[i]->SetBinContent(j + 1, inverted_sig); 
                    gFill[i]->SetPoint(j + 1, j * 2.0, inverted_sig); 
                    
                    if (inverted_sig > 0) qSum += inverted_sig;
                    if (inverted_sig > maxAmp) {
                        maxAmp = inverted_sig;
                        maxIdx = j;
                    }
                }
                gFill[i]->SetPoint(recordLength + 1, (recordLength - 1) * 2.0, 0); 
                
                double peakTime = maxIdx * 2.0; 

                hWave[i]->SetTitle(Form("Event %u - Channel %d (Base: %.1f);Time (ns);Signal Amplitude (ADC)", eventID, i, baseline));
                hWave[i]->GetYaxis()->SetRangeUser(-100, 4200); 
                
                hWave[i]->Draw("HIST"); 
                gFill[i]->Draw("F SAME"); 

                if (lPed[i]) delete lPed[i];
                lPed[i] = new TLine(0, 0, (recordLength - 1) * 2.0, 0);
                lPed[i]->SetLineColor(kRed);
                lPed[i]->SetLineStyle(2);
                lPed[i]->Draw();
                
                printf(" \033[1;33m[Ch %d]\033[0m Bsl: %6.1f | Amp: %6.1f | \033[1;32mTime: %6.1f ns\033[0m | Charge: %6.1f \n", i, baseline, maxAmp, peakTime, qSum);
            }
            c1->Modified();
            c1->Update();

            bool requires_rewind = false;
            while (true) {
                std::cout << "\033[1;36mEvent " << eventID << " Loaded.\033[0m [ENTER]: Next | [p]: Prev | [q]: Quit | [j]: Jump -> " << std::flush;
                
                while (!kbhit()) {
                    gSystem->ProcessEvents();
                    usleep(10000);
                }

                std::string input;
                std::getline(std::cin, input);
                
                if (input.empty() || input == "n" || input == "N") {
                    targetEventID = eventID + 1;
                    break;
                } 
                else if (input == "p" || input == "P") { 
                    if (eventID > 0) {
                        rewind(fp); 
                        eventID = 0;
                        targetEventID = targetEventID - 1; 
                        requires_rewind = true;
                        break;
                    } else {
                        std::cout << "\033[1;31mAlready at the first event.\033[0m\n";
                    }
                }
                else if (input == "q" || input == "Q") {
                    std::cout << "\n\033[1;33mUser requested exit.\033[0m\n";
                    fclose(fp);
                    return 0;
                } 
                else if (input == "j" || input == "J") {
                    std::cout << "Enter target event number: ";
                    std::string destStr;
                    std::getline(std::cin, destStr);
                    try {
                        int jump_idx = std::stoi(destStr);
                        if (jump_idx < 0) {
                            std::cout << "\033[1;31mEvent number cannot be negative.\033[0m\n";
                        } else if (jump_idx <= (int)eventID) {
                            rewind(fp); 
                            eventID = 0;
                            targetEventID = jump_idx;
                            requires_rewind = true;
                            break;
                        } else {
                            targetEventID = jump_idx; 
                            break;
                        }
                    } catch (...) {
                        std::cout << "\033[1;31mInvalid number.\033[0m\n";
                    }
                }
            }

            if (requires_rewind) continue; 
            eventID++;
        }

        std::cout << "\n\033[1;32m[ Interactive Display Terminated ]\033[0m\n";
        fclose(fp);
        return 0;
    }

    return 0;
}
