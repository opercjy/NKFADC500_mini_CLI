#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <algorithm>

#include "TApplication.h"
#include "TROOT.h"
#include "TCanvas.h"
#include "TH1I.h"
#include "TH1F.h"
#include "TLine.h"
#include "TSystem.h"
#include "TAxis.h"
#include "ELog.hh"

int main(int argc, char** argv) {
    std::cout << "\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m   NKFADC500 Mini - LIVE Online Monitor (Leak-Free)\033[0m\n";
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
    ELog::Print(ELog::INFO, "Auto-refreshing at 10 FPS. Close the window to safely stop.");

    TApplication app("app", &argc, argv);
    TCanvas* c1 = new TCanvas("c1", "FADC500 LIVE Waveform & Spectrum Monitor", 1600, 800);
    c1->Divide(4, 2);

    TH1I* hWave[4];
    TH1F* hSpec[4];
    TLine* lBase[4];

    for (int i = 0; i < 4; i++) {
        hWave[i] = new TH1I(Form("hWave_%d", i), Form("Channel %d Live;Time (ns);ADC Count", i), 1024, 0, 2048);
        hWave[i]->SetLineColor(kAzure + 2);
        hWave[i]->SetLineWidth(2);
        hWave[i]->SetStats(0);

        hSpec[i] = new TH1F(Form("hSpec_%d", i), Form("Channel %d Charge Spectrum;Charge (ADC);Counts", i), 500, 0, 30000);
        hSpec[i]->SetLineColor(kRed + 1);
        hSpec[i]->SetFillColor(kRed - 9);
        hSpec[i]->SetFillStyle(1001);
        hSpec[i]->SetMinimum(0.5); 

        lBase[i] = new TLine();
        lBase[i]->SetLineColor(kRed);
        lBase[i]->SetLineStyle(2);
        lBase[i]->SetLineWidth(2);

        c1->cd(i + 1);
        hWave[i]->Draw("HIST");
        lBase[i]->Draw("SAME");

        c1->cd(i + 5);
        gPad->SetLogy();
        hSpec[i]->Draw("HIST");
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

        size_t bytes_read = fread(header, 1, 128, fp);
        
        if (bytes_read < 128) { 
            clearerr(fp); 
            gSystem->ProcessEvents(); 
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); 
            // 💡 [수정 완료] static_cast<long> 추가
            if (bytes_read > 0) fseek(fp, -static_cast<long>(bytes_read), SEEK_CUR);
            continue;
        }

        unsigned int data_length = header[0];
        data_length += (header[4] << 8);
        data_length += (header[8] << 16);
        data_length += (header[12] << 24);

        if (data_length <= 32) break; 

        int num_samples = (data_length - 32) / 2; 
        int payload_bytes = num_samples * 8; 

        payload.resize(payload_bytes);
        bytes_read = fread(payload.data(), 1, payload_bytes, fp);
        
        if (bytes_read < (size_t)payload_bytes) {
            // 💡 [수정 완료] static_cast<long> 추가
            fseek(fp, -static_cast<long>(128 + bytes_read), SEEK_CUR); 
            clearerr(fp);
            gSystem->ProcessEvents();
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

        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - last_update).count();

        if (elapsed > 0.1) {
            for (int i = 0; i < 4; i++) {
                
                if (hWave[i]->GetNbinsX() != num_samples) {
                    hWave[i]->SetBins(num_samples, 0, num_samples * 2.0);
                }
                hWave[i]->Reset();

                double bslSum = 0;
                int nPed = std::min(20, num_samples);
                for(int pt=0; pt<nPed; pt++) bslSum += wave[i][pt];
                double bsl = (nPed > 0) ? (bslSum / nPed) : 0;

                double charge = 0;
                double minV = 99999, maxV = -99999;

                for (int pt = 0; pt < num_samples; pt++) {
                    unsigned short val = wave[i][pt];
                    hWave[i]->SetBinContent(pt + 1, val);

                    if (val > maxV) maxV = val;
                    if (val < minV) minV = val;

                    double drop = bsl - val; 
                    if (drop > 0) charge += drop;
                }

                if (charge > 0) hSpec[i]->Fill(charge);

                double margin = (maxV - minV) * 0.1;
                if (margin < 10) margin = 10;
                hWave[i]->GetYaxis()->SetRangeUser(minV - margin, maxV + margin);
                hWave[i]->SetTitle(Form("Ch %d Waveform (Event: %d);Time (ns);ADC Count", i, liveEventID));

                lBase[i]->SetX1(0);
                lBase[i]->SetY1(bsl);
                lBase[i]->SetX2(num_samples * 2.0); 
                lBase[i]->SetY2(bsl);

                c1->cd(i + 1);
                gPad->Modified(); 

                c1->cd(i + 5);
                gPad->Modified(); 
            }
            
            c1->Update(); 
            last_update = now;
        }

        gSystem->ProcessEvents();
    }

    fclose(fp);
    return 0;
}