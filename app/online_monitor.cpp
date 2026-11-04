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
#include "NoticeFadc500Spec.hh"
#include "TApplication.h"
#include "TAxis.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TH1I.h"
#include "TLine.h"
#include "TROOT.h"
#include "TString.h"
#include "TSystem.h"

namespace {
bool kbhit() {
    if (!isatty(STDIN_FILENO) || !std::cin.good()) return false;
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

    std::string inputFile;
    std::string explicitConfig;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-f" || arg == "--config") && i + 1 < argc) {
            explicitConfig = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: ./online_nkfadc500 <live_data_file.dat> [-f <settings.cfg>]\n";
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            inputFile = arg;
        }
    }

    if (inputFile.empty()) {
        ELog::Print(ELog::FATAL, "Usage: ./online_nkfadc500 <live_data_file.dat> [-f <settings.cfg>]");
        return 1;
    }

    FILE* fp = nullptr;
    for (int wait_iter = 0; wait_iter < 50; ++wait_iter) {
        fp = std::fopen(inputFile.c_str(), "rb");
        if (fp) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
    if (!fp) {
        ELog::Print(ELog::FATAL, Form("Cannot open live binary file: %s", inputFile.c_str()));
        return 1;
    }

    const std::string cfgPath =
        NoticeFadc500Spec::ResolveCompanionConfigPath(explicitConfig, inputFile, argv[0]);
    NoticeFadc500Spec::StreamDecodeConfig streamCfg =
        NoticeFadc500Spec::LoadStreamDecodeConfig(cfgPath);

    ELog::Print(ELog::INFO,
        Form("Tailing live DAQ stream: %s (cfg: %s, dt=%.1f ns, POL[0]=%d)",
             inputFile.c_str(),
             streamCfg.loaded_path.empty() ? "defaults" : streamCfg.loaded_path.c_str(),
             streamCfg.dt_ns, streamCfg.polarity[0]));

    TApplication app("app", &argc, argv);
    TCanvas* c1 = new TCanvas("c1", "FADC500 LIVE Waveform & Spectrum Profile Monitor", 1600, 860);
    c1->Divide(4, 2);

    TH1I* hWave[4];
    TH1F* hSpec[4];
    TLine* lBase[4];

    for (int i = 0; i < 4; i++) {
        hWave[i] = new TH1I(Form("hWave_%d", i), Form("Channel %d Live;Time (ns);ADC Count", i), 496, 0, 496 * streamCfg.dt_ns);
        hWave[i]->SetLineColor(kAzure + 2);
        hWave[i]->SetLineWidth(2);
        hWave[i]->SetStats(0);

        hSpec[i] = new TH1F(Form("hSpec_%d", i), Form("Channel %d Charge Profile;Charge (ADC*ns);Counts", i), 250, 0, 8000);
        hSpec[i]->SetCanExtend(TH1::kAllAxes);
        hSpec[i]->SetLineColor(kRed + 1);
        hSpec[i]->SetFillColor(kRed - 9);
        hSpec[i]->SetFillStyle(1001);
        hSpec[i]->SetMinimum(0.5);
        hSpec[i]->SetStats(1);

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
    auto run_start_time = std::chrono::steady_clock::now();
    auto last_update = run_start_time;

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
            ELog::Print(ELog::WARNING, "File truncation detected (New Run). Reloading companion config & auto-clearing...");
            std::rewind(fp);
            const std::string refreshedCfg =
                NoticeFadc500Spec::ResolveCompanionConfigPath(explicitConfig, inputFile, argv[0]);
            streamCfg = NoticeFadc500Spec::LoadStreamDecodeConfig(refreshedCfg);
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

        const auto hdr = NoticeFadc500Spec::DecodeHeader128(header);
        if (!hdr.valid_geometry || !hdr.valid_channel_tags) {
            ELog::Print(ELog::WARNING, Form("Invalid packet header (len=%u) at byte %ld; pausing monitor.", hdr.data_length, packet_start_pos));
            break;
        }

        const int num_samples = static_cast<int>((hdr.data_length - 32U) / 2U);
        const size_t payload_bytes = static_cast<size_t>(num_samples) * 8ULL;

        if (num_samples == 0) {
            // Zero-suppressed header-only packet
            liveEventID++;
            continue;
        }

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

        const double dt_ns = streamCfg.dt_ns;
        double bsl[4] = {0.0, 0.0, 0.0, 0.0};
        double minV[4] = {99999.0, 99999.0, 99999.0, 99999.0};
        double maxV[4] = {-99999.0, -99999.0, -99999.0, -99999.0};
        double peakAmp[4] = {0.0, 0.0, 0.0, 0.0};

        for (int i = 0; i < 4; i++) {
            double bslSum = 0.0;
            const int nPed = NoticeFadc500Spec::ComputeBaselineSamples(
                streamCfg.delay_ns[i], dt_ns, num_samples);
            for (int pt = 0; pt < nPed; pt++) bslSum += wave[i][pt];
            bsl[i] = (nPed > 0) ? (bslSum / static_cast<double>(nPed)) : 0.0;

            const int pol = streamCfg.polarity[i];
            double chargeNs = 0.0;
            for (int pt = 0; pt < num_samples; pt++) {
                const uint16_t val = wave[i][pt];
                if (val > maxV[i]) maxV[i] = val;
                if (val < minV[i]) minV[i] = val;
                const double sig = (pol == 0)
                    ? (bsl[i] - static_cast<double>(val))
                    : (static_cast<double>(val) - bsl[i]);
                if (sig > peakAmp[i]) peakAmp[i] = sig;
                if (sig > 0.0) chargeNs += sig * dt_ns;
            }
            const bool chTriggered = ((hdr.trigger_pattern & (0xFU << (i * 4))) != 0U) || (hdr.trigger_pattern == 0U);
            if (chargeNs > 0.0 && chTriggered) hSpec[i]->Fill(chargeNs);
        }

        auto now = std::chrono::steady_clock::now();
        const double elapsed = std::chrono::duration<double>(now - last_update).count();

        if (elapsed > 0.2) {
            const double totalRunSec = std::max(0.001, std::chrono::duration<double>(now - run_start_time).count());
            const double avgRateHz = static_cast<double>(liveEventID) / totalRunSec;
            const double xMaxNs = num_samples * dt_ns;
            for (int i = 0; i < 4; i++) {
                if (hWave[i]->GetNbinsX() != num_samples ||
                    std::abs(hWave[i]->GetXaxis()->GetXmax() - xMaxNs) > 1e-3) {
                    hWave[i]->SetBins(num_samples, 0.0, xMaxNs);
                }
                hWave[i]->Reset();
                for (int pt = 0; pt < num_samples; pt++) {
                    hWave[i]->SetBinContent(pt + 1, wave[i][pt]);
                }

                double margin = (maxV[i] - minV[i]) * 0.1;
                if (margin < 10.0) margin = 10.0;
                hWave[i]->GetYaxis()->SetRangeUser(minV[i] - margin, maxV[i] + margin);
                hWave[i]->SetTitle(Form("Ch %d Live (Evt %u | Amp %.1f ADC | %.1f Hz);Time (ns);ADC Count",
                                        i, liveEventID, peakAmp[i], avgRateHz));

                lBase[i]->SetX1(0.0);
                lBase[i]->SetY1(bsl[i]);
                lBase[i]->SetX2(xMaxNs);
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
