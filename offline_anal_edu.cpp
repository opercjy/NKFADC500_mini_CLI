#include <algorithm>
#include <iostream>
#include "TCanvas.h"
#include "TCut.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TLegend.h"
#include "TLeaf.h"
#include "TLine.h"
#include "TPaveText.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TTree.h"

namespace {
TString ResolveEduBranch(TTree* tree, const char* base, int ch) {
    TString scalarName = Form("%s_Ch%d", base, ch);
    if (tree->GetBranch(scalarName)) return scalarName;
    return Form("%s[%d]", base, ch);
}
} // namespace

// NOTICE NKFADC500 Mini nominal input: 12-bit (4096), 2.5 Vpp (+/-1.25 V into 50 Ohm), 500 MS/s (2.0 ns/sample)
// -> 1 ADC = 0.61035 mV, 1 ADC*sample = (2.5 V / 4096) * (2.0 ns / 50 Ohm) = 0.0244140625 pC
void offline_anal_edu(const char* filename = "data/run_101_prod.root",
                      int targetCh = 0,
                      double ampCut = 50.0,
                      double dtCut_us = 10.0,
                      double vpp = 2.5) {
    if (targetCh < 0 || targetCh > 3) {
        std::cerr << "\033[1;31m[ERROR]\033[0m targetCh must be in [0, 3]." << std::endl;
        return;
    }

    gStyle->SetOptStat(111111);
    gStyle->SetPalette(kBird);
    if (!gSystem->AccessPathName("lib/libFADC500Objects.so")) {
        gSystem->Load("lib/libFADC500Objects.so");
    }

    TFile* f = TFile::Open(filename, "READ");
    if (!f || f->IsZombie()) {
        std::cerr << "\033[1;31m[ERROR]\033[0m Cannot open file: " << filename << std::endl;
        return;
    }
    TTree* tree = dynamic_cast<TTree*>(f->Get("PROD"));
    if (!tree || tree->GetEntries() == 0) {
        std::cerr << "\033[1;31m[ERROR]\033[0m PROD tree not found or empty!" << std::endl;
        return;
    }

    int ndp = 496;
    double dt_ns = 2.0;
    if (tree->GetBranch("RecordLength")) tree->SetBranchAddress("RecordLength", &ndp);
    if (tree->GetBranch("SamplingNs"))   tree->SetBranchAddress("SamplingNs", &dt_ns);
    tree->GetEntry(0);
    tree->ResetBranchAddresses();
    if (ndp <= 0) ndp = 496;
    if (dt_ns <= 0.0) dt_ns = 2.0;

    const double maxTime = ndp * dt_ns;
    const double adc_to_pc = (vpp / 4096.0) * (dt_ns / 50.0) * 1000.0; // pC per (ADC * sample)

    const TString brAmp = ResolveEduBranch(tree, "Amplitude", targetCh);
    const TString brChg = ResolveEduBranch(tree, "Charge", targetCh);
    const TString brWaveDrop = Form("wDrop_Ch%d", targetCh);
    const TString brWaveTime = Form("wTime_Ch%d", targetCh);
    const TString brPsd = Form("PsdRatio_Ch%d", targetCh);
    const TString brQtot = Form("Qtotal_Ch%d", targetCh);

    const double obsMaxAmp = std::max(100.0, tree->GetMaximum(brAmp.Data()));
    const double obsMaxPc  = std::max(20.0, tree->GetMaximum(brChg.Data()) * adc_to_pc);
    const double maxAmpAxis = std::min(4200.0, std::max(250.0, (obsMaxAmp > 300.0 ? 280.0 : obsMaxAmp * 1.15)));
    const double maxPcAxis  = std::min(1500.0, std::max(45.0, (obsMaxPc > 60.0 ? 50.0 : obsMaxPc * 1.15)));

    TCanvas* c1 = new TCanvas("c1", "NKFADC500 Educational Analysis Dashboard", 1600, 1000);
    c1->Divide(3, 2);

    // 1. Waveform Persistence or PSD Scatter
    c1->cd(1);
    gPad->SetGrid();
    gPad->SetRightMargin(0.12);
    if (tree->GetBranch(brWaveDrop) && tree->GetBranch(brWaveTime)) {
        tree->Draw(Form("%s:%s>>hWave2D(%d, 0, %f, 200, -30, %f)",
                        brWaveDrop.Data(), brWaveTime.Data(), ndp, maxTime, maxAmpAxis), "", "colz");
        if (TH2F* hWave2D = dynamic_cast<TH2F*>(gDirectory->Get("hWave2D"))) {
            hWave2D->SetTitle(Form("1. Waveform Persistence (Ch %d);Time (ns);Voltage Drop (ADC)", targetCh));
            hWave2D->SetStats(0);
        }
    } else if (tree->GetBranch(brPsd) && tree->GetBranch(brQtot)) {
        tree->Draw(Form("%s:%s>>hPsdEdu(200, 0, 10000, 120, 0.0, 1.0)",
                        brPsd.Data(), brQtot.Data()), "", "colz");
        if (TH2F* hPsdEdu = dynamic_cast<TH2F*>(gDirectory->Get("hPsdEdu"))) {
            hPsdEdu->SetTitle(Form("1. Charge-Comparison PSD (Ch %d);Q_{total} (ADC*sample);Q_{tail} / Q_{total}", targetCh));
            hPsdEdu->SetStats(0);
        }
    }

    // 2. Amplitude Distribution
    c1->cd(2);
    gPad->SetGrid();
    gPad->SetLogy();
    tree->Draw(Form("%s>>hAmp(150, 0, %f)", brAmp.Data(), maxAmpAxis), "", "HIST");
    if (TH1F* hAmp = dynamic_cast<TH1F*>(gDirectory->Get("hAmp"))) {
        hAmp->SetTitle("2. Amplitude Distribution;Amplitude (ADC);Counts");
        hAmp->SetFillColor(kAzure - 9);
        TLine* lineAmp = new TLine(ampCut, 0.5, ampCut, std::max(1.0, hAmp->GetMaximum()));
        lineAmp->SetLineColor(kRed);
        lineAmp->SetLineWidth(2);
        lineAmp->SetLineStyle(2);
        lineAmp->Draw("SAME");
    }

    // 3. Delta T (Inter-event arrival time difference, guarded against unsigned wrap)
    c1->cd(3);
    gPad->SetGrid();
    gPad->SetLogy();
    TH1F* hDeltaT = new TH1F("hDeltaT", "3. Time Between Events (#Delta t);#Delta t (#mu s);Counts", 120, 0, 40000);
    hDeltaT->SetFillColor(kTeal - 9);

    if (TBranch* brTrig = tree->GetBranch("LocalTriggerTime")) {
        TLeaf* lfTrig = tree->GetLeaf("LocalTriggerTime");
        const Long64_t nEntries = tree->GetEntries();
        ULong64_t prevTime = 0;
        bool isFirst = true;

        for (Long64_t i = 0; i < nEntries; ++i) {
            brTrig->GetEntry(i);
            if (!lfTrig) break;
            const Long64_t rawVal = lfTrig->GetValueLong64();
            if (rawVal < 0) continue;
            const ULong64_t currTime = static_cast<ULong64_t>(rawVal);
            if (!isFirst && currTime >= prevTime) {
                const double dt_us = static_cast<double>(currTime - prevTime) / 1000.0;
                hDeltaT->Fill(dt_us);
            }
            prevTime = currTime;
            isFirst = false;
        }
    }
    hDeltaT->Draw("HIST");
    TLine* lineDt = new TLine(dtCut_us, 0.5, dtCut_us, std::max(1.0, hDeltaT->GetMaximum()));
    lineDt->SetLineColor(kMagenta);
    lineDt->SetLineWidth(2);
    lineDt->SetLineStyle(2);
    lineDt->Draw("SAME");

    // 4. Charge Spectrum in pC
    c1->cd(4);
    gPad->SetGrid();
    gPad->SetLogy();
    tree->Draw(Form("%s * %.10f>>hChargeRaw(150, 0, %f)", brChg.Data(), adc_to_pc, maxPcAxis), "", "HIST");
    TH1F* hChargeRaw = dynamic_cast<TH1F*>(gDirectory->Get("hChargeRaw"));
    if (hChargeRaw) {
        hChargeRaw->SetTitle("4. Charge Spectrum (Raw vs Amp Cut);Charge (pC);Counts");
        hChargeRaw->SetLineColor(kGray + 1);
        hChargeRaw->SetFillColor(kGray + 1);
        hChargeRaw->SetFillStyle(3004);
    }

    TCut cutAmp = Form("%s > %f", brAmp.Data(), ampCut);
    tree->Draw(Form("%s * %.10f>>hChargeCut(150, 0, %f)", brChg.Data(), adc_to_pc, maxPcAxis), cutAmp, "HIST SAME");
    TH1F* hChargeCut = dynamic_cast<TH1F*>(gDirectory->Get("hChargeCut"));
    if (hChargeCut) {
        hChargeCut->SetLineColor(kRed + 1);
        hChargeCut->SetFillColor(kRed - 9);
        hChargeCut->SetFillStyle(1001);
    }

    TLegend* leg1 = new TLegend(0.34, 0.74, 0.66, 0.88);
    if (hChargeRaw) leg1->AddEntry(hChargeRaw, "Raw (All)", "f");
    if (hChargeCut) leg1->AddEntry(hChargeCut, Form("Amp > %.0f ADC", ampCut), "f");
    leg1->Draw();

    // 5. Amplitude vs Charge 2D Scatter
    c1->cd(5);
    gPad->SetGrid();
    gPad->SetRightMargin(0.12);
    tree->Draw(Form("%s:(%s * %.10f)>>h2D(150, 0, %f, 150, 0, %f)",
                    brAmp.Data(), brChg.Data(), adc_to_pc, maxPcAxis, maxAmpAxis), "", "colz");
    if (TH2F* h2D = dynamic_cast<TH2F*>(gDirectory->Get("h2D"))) {
        h2D->SetTitle("5. Amp vs Charge Scatter;Charge (pC);Amplitude (ADC)");
        h2D->SetStats(0);
    }

    // 6. Educational Hardware & Physics Summary
    c1->cd(6);
    TPaveText* pt = new TPaveText(0.08, 0.10, 0.92, 0.90);
    pt->AddText("[NKFADC500 Mini Educational Summary]");
    pt->AddText(" ");
    pt->AddText(Form("- Hardware: 12-bit (4096), %.1f Vpp (50 #Omega), %.0f MS/s", vpp, 1000.0 / dt_ns));
    pt->AddText(Form("- 1 ADC = %.4f mV | 1 ADC*sample = %.5f pC", (vpp * 1000.0) / 4096.0, adc_to_pc));
    pt->AddText(Form("- Target Channel : %d (N_{s} = %d samples)", targetCh, ndp));
    pt->AddText(Form("- Amp Cut: > %.1f ADC (Suppresses Baseline Noise)", ampCut));
    pt->AddText(Form("- Time Cut: > %.1f #mus (Suppresses PMT Afterpulses)", dtCut_us));
    pt->SetFillColor(kWhite);
    pt->SetShadowColor(kWhite);
    pt->SetTextFont(42);
    pt->SetTextSize(0.042);
    pt->Draw();

    c1->Modified();
    c1->Update();
}
