#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <sys/select.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "ELog.hh"
#include "TAxis.h"
#include "TApplication.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TH1I.h"
#include "TLine.h"
#include "TROOT.h"
#include "TSystem.h"

namespace {
bool kbhit() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}
} // namespace

int main(int argc, char** argv) {
    std::cout << "\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m   NKFADC500 Mini - LIVE Online Monitor (Auto-Clear)\033[0m\n";
    std::cout << "\033[1;36m========================================================\033[0m\n\n";

    if (argc < 2) {
        ELog::Print(ELog::FATAL, "Usage: ./online_nkfadc500 <live_data_file.dat>");
        return 1;
    }

    std::string inputFile = argv[1];

    FILE* fp = std::fopen(inputFile.c_str(), "rb");
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
        hWave[i]->SetLineColor(kAzure + 2);
        hWave[i]->SetLineWidth(2);
        hWave[i]->SetStats(0);

        hSpec[i] = new TH1F(Form("hSpec_%d", i), Form("Channel %d Charge Spectrum;Charge (ADC*sample);Counts", i), 500, 0, 30000);
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
    std::vector<uint16_t> wave[4];

    while (true) {
        if (!gROOT->GetListOfCanvases()->FindObject("c1")) {
            ELog::Print(ELog::INFO, "Monitor window closed by user. Shutting down gracefully...");
            break;
        }

        if (kbhit()) {
            char cmd = 0;
            if (std::cin >> cmd) {
                if (cmd == 'q' || cmd == 'Q') break;
                if (cmd == 'c' || cmd == 'C') {
                    ELog::Print(ELog::INFO, "Clear command received. Resetting Histograms...");
                    for (int i = 0; i < 4; i++) {
                        hWave[i]->Reset();
                        hSpec[i]->Reset();
                    }
                    c1->Update();
                    liveEventID = 0;
                }
            }
        }

        const long packet_start_pos = std::ftell(fp);
        std::fseek(fp, 0, SEEK_END);
        const long file_size = std::ftell(fp);
        std::fseek(fp, packet_start_pos, SEEK_SET);

        if (file_size < packet_start_pos) {
            ELog::Print(ELog::WARNING, "File truncation detected (New Run). Auto-clearing...");
            std::rewind(fp);
            for (int i = 0; i < 4; i++) {
                hWave[i]->Reset();
                hSpec[i]->Reset();
            }
            c1->Update();
            liveEventID = 0;
            continue;
        }

        const size_t hdr_read = std::fread(header, 1, 128, fp);
        if (hdr_read < 128) {
            std::clearerr(fp);
            std::fseek(fp, packet_start_pos, SEEK_SET);
            gSystem->ProcessEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        const uint32_t data_length =
            static_cast<uint32_t>(header[0]) |
            (static_cast<uint32_t>(header[4]) << 8) |
            (static_cast<uint32_t>(header[8]) << 16) |
            (static_cast<uint32_t>(header[12]) << 24);
        if (data_length <= 32U || data_length > 100000000U || ((data_length - 32U) % 2U != 0U)) {
            ELog::Print(ELog::WARNING, Form("Invalid packet length (%u) at byte %ld; pausing monitor.", data_length, packet_start_pos));
            break;
        }

        const int num_samples = static_cast<int>((data_length - 32U) / 2U);
        const size_t payload_bytes = static_cast<size_t>(num_samples) * 8ULL;

        payload.resize(payload_bytes);
        if (std::fread(payload.data(), 1, payload_bytes, fp) < payload_bytes) {
            // Exact rollback to packet_start_pos (fixes partial-read stream desync bug!)
            std::clearerr(fp);
            std::fseek(fp, packet_start_pos, SEEK_SET);
            gSystem->ProcessEvents();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            continue;
        }

        liveEventID++;

        for (int i = 0; i < 4; i++) {
            wave[i].resize(num_samples);
        }

        for (int j = 0; j < num_samples; j++) {
            const size_t offset = static_cast<size_t>(j) * 8ULL;
            wave[0][j] = static_cast<uint16_t>((payload[offset + 0] | (payload[offset + 4] << 8)) & 0x0FFFU);
            wave[1][j] = static_cast<uint16_t>((payload[offset + 1] | (payload[offset + 5] << 8)) & 0x0FFFU);
            wave[2][j] = static_cast<uint16_t>((payload[offset + 2] | (payload[offset + 6] << 8)) & 0x0FFFU);
            wave[3][j] = static_cast<uint16_t>((payload[offset + 3] | (payload[offset + 7] << 8)) & 0x0FFFU);
        }

        double bsl[4] = {0.0, 0.0, 0.0, 0.0};
        double minV[4] = {99999.0, 99999.0, 99999.0, 99999.0};
        double maxV[4] = {-99999.0, -99999.0, -99999.0, -99999.0};

        for (int i = 0; i < 4; i++) {
            double bslSum = 0.0;
            const int nPed = std::min(20, num_samples);
            for (int pt = 0; pt < nPed; pt++) bslSum += wave[i][pt];
            bsl[i] = (nPed > 0) ? (bslSum / static_cast<double>(nPed)) : 0.0;

            double charge = 0.0;
            for (int pt = 0; pt < num_samples; pt++) {
                const uint16_t val = wave[i][pt];
                if (val > maxV[i]) maxV[i] = val;
                if (val < minV[i]) minV[i] = val;
                const double drop = bsl[i] - static_cast<double>(val);
                if (drop > 0.0) charge += drop;
            }
            if (charge > 0.0) hSpec[i]->Fill(charge);
        }

        auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - last_update).count();

        if (elapsed > 0.1) {
            for (int i = 0; i < 4; i++) {
                if (hWave[i]->GetNbinsX() != num_samples) {
                    hWave[i]->SetBins(num_samples, 0.0, num_samples * 2.0);
                }
                hWave[i]->Reset();
                for (int pt = 0; pt < num_samples; pt++) {
                    hWave[i]->SetBinContent(pt + 1, wave[i][pt]);
                }

                double margin = (maxV[i] - minV[i]) * 0.1;
                if (margin < 10.0) margin = 10.0;
                hWave[i]->GetYaxis()->SetRangeUser(minV[i] - margin, maxV[i] + margin);
                hWave[i]->SetTitle(Form("Ch %d Waveform (Event: %u);Time (ns);ADC Count", i, liveEventID));

                lBase[i]->SetX1(0.0);
                lBase[i]->SetY1(bsl[i]);
                lBase[i]->SetX2(num_samples * 2.0);
                lBase[i]->SetY2(bsl[i]);

                c1->cd(i + 1);
                gPad->Modified();
                c1->cd(i + 5);
                gPad->Modified();
            }
            c1->Update();
            gSystem->ProcessEvents();
            last_update = now;
        }
    }

    std::fclose(fp);
    return 0;
}
