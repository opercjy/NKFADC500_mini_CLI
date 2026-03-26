#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>
#include <sys/select.h> // 💡 STDIN 비동기 입력 제어용

#include "TApplication.h"
#include "TROOT.h"
#include "TCanvas.h"
#include "TH1I.h"
#include "TH1F.h"
#include "TLine.h"
#include "TSystem.h"
#include "TAxis.h"
#include "ELog.hh"

// 💡 [핵심 픽스] 비동기 키보드 및 파이프 입력 감지
bool kbhit() {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}

int main(int argc, char** argv) {
    std::cout << "\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m   NKFADC500 Mini - LIVE Online Monitor (Auto-Clear)\033[0m\n";
    std::cout << "\033[1;36m========================================================\033[0m\n\n";

    if (argc < 2) {
        ELog::Print(ELog::FATAL, "Usage: ./online_monitor <live_data_file.dat>");
        return 1;
    }

    std::string inputFile = argv[1];
    
    FILE* fp = fopen(inputFile.c_str(), "rb");
    if (!fp) {
        ELog::Print(ELog::FATAL, Form("Cannot open live binary file: %s", inputFile.c_str()));
        return 1;
    }

    ELog::Print(ELog::INFO, Form("Tailing live DAQ stream : %s", inputFile.c_str()));

    TApplication app("app", &argc, argv);
    TCanvas* c1 = new TCanvas("c1", "FADC500 LIVE Waveform & Spectrum Monitor", 1600, 800);
    c1->Divide(4, 2);

    TH1I* hWave[4];
    TH1F* hSpec[4];
    TLine* lBase[4];

    for (int i = 0; i < 4; i++) {
        hWave[i] = new TH1I(Form("hWave_%d", i), Form("Channel %d Live;Time (ns);ADC Count", i), 1024, 0, 2048);
        hWave[i]->SetLineColor(kAzure + 2); hWave[i]->SetLineWidth(2); hWave[i]->SetStats(0);

        hSpec[i] = new TH1F(Form("hSpec_%d", i), Form("Channel %d Charge Spectrum;Charge (ADC);Counts", i), 500, 0, 30000);
        hSpec[i]->SetLineColor(kRed + 1); hSpec[i]->SetFillColor(kRed - 9); hSpec[i]->SetFillStyle(1001); hSpec[i]->SetMinimum(0.5); 

        lBase[i] = new TLine();
        lBase[i]->SetLineColor(kRed); lBase[i]->SetLineStyle(2); lBase[i]->SetLineWidth(2);

        c1->cd(i + 1); hWave[i]->Draw("HIST"); lBase[i]->Draw("SAME");
        c1->cd(i + 5); gPad->SetLogy(); hSpec[i]->Draw("HIST");
    }
    c1->Update();

    unsigned char header[128];
    unsigned int liveEventID = 0;
    auto last_update = std::chrono::steady_clock::now();

    std::vector<unsigned char> payload;
    std::vector<unsigned short> wave[4];

    while (true) {
        if (!gROOT->GetListOfCanvases()->FindObject("c1")) {
            ELog::Print(ELog::INFO, "Monitor window closed by user. Shutting down gracefully...");
            break;
        }

        // 💡 [핵심 픽스] GUI 수동 리프레시 명령('c') 또는 종료 명령('q') 감지
        if (kbhit()) {
            char cmd; std::cin >> cmd;
            if (cmd == 'q') break;
            else if (cmd == 'c') {
                ELog::Print(ELog::INFO, "Clear command received. Resetting Histograms...");
                for(int i=0; i<4; i++) { hWave[i]->Reset(); hSpec[i]->Reset(); }
                c1->Update(); liveEventID = 0;
            }
        }

        // 💡 [핵심 픽스] 동일한 파일에 덮어쓰기가 발생하여 파일 크기가 줄어들었을 때 자동 리셋
        long current_pos = ftell(fp);
        fseek(fp, 0, SEEK_END);
        long file_size = ftell(fp);
        fseek(fp, current_pos, SEEK_SET);

        if (file_size < current_pos) {
            ELog::Print(ELog::WARNING, "File truncation detected (New Run). Auto-clearing...");
            rewind(fp);
            for(int i=0; i<4; i++) { hWave[i]->Reset(); hSpec[i]->Reset(); }
            c1->Update(); liveEventID = 0;
            continue;
        }

        size_t bytes_read = fread(header, 1, 128, fp);
        
        if (bytes_read < 128) { 
            clearerr(fp); 
            gSystem->ProcessEvents(); 
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); 
            if (bytes_read > 0) fseek(fp, -static_cast<long>(bytes_read), SEEK_CUR);
            continue;
        }

        unsigned int data_length = header[0] + (header[4] << 8) + (header[8] << 16) + (header[12] << 24);
        if (data_length <= 32) break; 

        int num_samples = (data_length - 32) / 2; 
        int payload_bytes = num_samples * 8; 

        payload.resize(payload_bytes);
        if (fread(payload.data(), 1, payload_bytes, fp) < (size_t)payload_bytes) {
            fseek(fp, -static_cast<long>(128 + payload_bytes), SEEK_CUR); 
            clearerr(fp); gSystem->ProcessEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        liveEventID++;

        for(int i=0; i<4; i++) {
            wave[i].clear();
            if (wave[i].capacity() < static_cast<size_t>(num_samples)) wave[i].reserve(num_samples);
        }

        for (int j = 0; j < num_samples; j++) {
            int offset = j * 8;
            wave[0].push_back((payload[offset + 0] | (payload[offset + 4] << 8)) & 0x0FFF);
            wave[1].push_back((payload[offset + 1] | (payload[offset + 5] << 8)) & 0x0FFF);
            wave[2].push_back((payload[offset + 2] | (payload[offset + 6] << 8)) & 0x0FFF);
            wave[3].push_back((payload[offset + 3] | (payload[offset + 7] << 8)) & 0x0FFF);
        }

        double bsl[4] = {0};
        double minV[4] = {99999, 99999, 99999, 99999};
        double maxV[4] = {-99999, -99999, -99999, -99999};

        for (int i = 0; i < 4; i++) {
            double bslSum = 0; int nPed = std::min(20, num_samples);
            for(int pt=0; pt<nPed; pt++) bslSum += wave[i][pt];
            bsl[i] = (nPed > 0) ? (bslSum / nPed) : 0;

            double charge = 0;
            for (int pt = 0; pt < num_samples; pt++) {
                unsigned short val = wave[i][pt];
                if (val > maxV[i]) maxV[i] = val;
                if (val < minV[i]) minV[i] = val;
                double drop = bsl[i] - val; 
                if (drop > 0) charge += drop;
            }
            if (charge > 0) hSpec[i]->Fill(charge);
        }

        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_update).count();

        if (elapsed > 0.1) {
            for (int i = 0; i < 4; i++) {
                if (hWave[i]->GetNbinsX() != num_samples) hWave[i]->SetBins(num_samples, 0, num_samples * 2.0);
                hWave[i]->Reset();
                for (int pt = 0; pt < num_samples; pt++) hWave[i]->SetBinContent(pt + 1, wave[i][pt]); 

                double margin = (maxV[i] - minV[i]) * 0.1;
                if (margin < 10) margin = 10;
                hWave[i]->GetYaxis()->SetRangeUser(minV[i] - margin, maxV[i] + margin);
                hWave[i]->SetTitle(Form("Ch %d Waveform (Event: %d);Time (ns);ADC Count", i, liveEventID));

                lBase[i]->SetX1(0); lBase[i]->SetY1(bsl[i]);
                lBase[i]->SetX2(num_samples * 2.0); lBase[i]->SetY2(bsl[i]);

                c1->cd(i + 1); gPad->Modified(); 
                c1->cd(i + 5); gPad->Modified(); 
            }
            c1->Update(); 
            gSystem->ProcessEvents(); 
            last_update = now;
        }
    }

    fclose(fp);
    return 0;
}