#include <algorithm>
#include <iostream>
#include <string>
#include "TCanvas.h"
#include "TCut.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TLeaf.h"
#include "TLegend.h"
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

TString ResolveDefaultEduFile(const char* requested) {
    if (requested && !gSystem->AccessPathName(requested)) {
        return TString(requested);
    }
    const char* fallbacks[] = {
        "data/20261104_160_temp_EDA_CA_1000_ml_per_l_60co_hv_1700_thr_30_min_20_prod.root",
        "data/20261104_160_temp_EDA_CA_50_ml_per_l_60co_hv_1700_thr_30_min_20_prod.root",
        "data/hv1700_ch0_thr40_prod.root",
        "data/run_101_prod.root"
    };
    for (const char* fb : fallbacks) {
        if (!gSystem->AccessPathName(fb)) return TString(fb);
    }
    return TString(requested ? requested : "");
}
} // namespace

// NOTICE NKFADC500 Mini nominal input: 12-bit (4096), 2.5 Vpp (+/-1.25 V into 50 Ohm), 500 MS/s (2.0 ns/sample)
// -> 1 ADC = 0.6103515625 mV, 1 ADC*sample = (2.5 V / 4096) * (2.0 ns / 50 Ohm) = 0.0244140625 pC
void offline_anal_edu(const char* filename = "data/20261104_160_temp_EDA_CA_1000_ml_per_l_60co_hv_1700_thr_30_min_20_prod.root",
                      int targetCh = 0,
                      double ampCut = 45.0,
                      double dtCut_us = 250.0,
                      double vpp = 2.5,
                      const char* savePng = "") {
    if (targetCh < 0 || targetCh > 3) {
        std::cerr << "\033[1;31m[ERROR]\033[0m targetCh must be in [0, 3]." << std::endl;
        return;
    }

    gStyle->SetOptStat(111111);
    gStyle->SetPalette(kBird);
    if (!gSystem->AccessPathName("lib/libFADC500Objects.so")) {
        gSystem->Load("lib/libFADC500Objects.so");
    }

    const TString resolvedFile = ResolveDefaultEduFile(filename);
    TFile* f = TFile::Open(resolvedFile.Data(), "READ");
    if (!f || f->IsZombie()) {
        std::cerr << "\033[1;31m[ERROR]\033[0m Cannot open file: " << resolvedFile << std::endl;
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
    const double adc_to_pc = (vpp / 4096.0) * (dt_ns / 50.0) * 1000.0; // 0.0244140625 pC per (ADC * sample)

    const TString brAmp = ResolveEduBranch(tree, "Amplitude", targetCh);
    const TString brChg = ResolveEduBranch(tree, "Charge", targetCh);
    const TString brWaveDrop = Form("wDrop_Ch%d", targetCh);
    const TString brWaveTime = Form("wTime_Ch%d", targetCh);
    const TString brPsd = Form("PsdRatio_Ch%d", targetCh);
    const TString brQtot = Form("Qtotal_Ch%d", targetCh);

    const double maxAmpAxis = 350.0;
    const double maxPcAxis  = 45.0; // Focus on 60Co Compton continuum & edge (0..45 pC)

    TCanvas* c1 = new TCanvas("c1", "NKFADC500 Educational Analysis Dashboard (60Co Source)", 1650, 1020);
    c1->Divide(3, 2);

    // 1. Waveform Persistence (LogZ) or PSD Scatter
    c1->cd(1);
    gPad->SetGrid();
    gPad->SetLogz();
    gPad->SetRightMargin(0.13);
    if (tree->GetBranch(brWaveDrop) && tree->GetBranch(brWaveTime)) {
        tree->Draw(Form("%s:%s>>hWave2D(%d, 0, %f, 200, -25, %f)",
                        brWaveDrop.Data(), brWaveTime.Data(), ndp, maxTime, maxAmpAxis),
                   "Entry$ < 25000", "colz");
        if (TH2F* hWave2D = dynamic_cast<TH2F*>(gDirectory->Get("hWave2D"))) {
            hWave2D->SetTitle(Form("1. Waveform Persistence (Ch %d, ^{60}Co #gamma);Time (ns);Voltage Drop (ADC)", targetCh));
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
        hAmp->SetTitle("2. Pulse Amplitude Distribution (^{60}Co);Amplitude (ADC) [0.61 mV/ADC];Counts");
        hAmp->SetLineColor(kAzure + 2);
        hAmp->SetFillColor(kAzure - 9);
        TLine* lineAmp = new TLine(ampCut, 0.5, ampCut, std::max(1.0, hAmp->GetMaximum() * 1.5));
        lineAmp->SetLineColor(kRed + 1);
        lineAmp->SetLineWidth(2);
        lineAmp->SetLineStyle(2);
        lineAmp->Draw("SAME");
    }

    // 3. Delta T (Inter-event arrival time difference -> Poisson exponential decay)
    c1->cd(3);
    gPad->SetGrid();
    gPad->SetLogy();
    TH1F* hDeltaT = new TH1F("hDeltaT", "3. Inter-Event Arrival Time (#Delta t, Poisson Process);#Delta t (#mu s);Counts", 120, 0, 45000);
    hDeltaT->SetLineColor(kTeal + 3);
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
    TLine* lineDt = new TLine(dtCut_us, 0.5, dtCut_us, std::max(1.0, hDeltaT->GetMaximum() * 1.5));
    lineDt->SetLineColor(kMagenta + 1);
    lineDt->SetLineWidth(2);
    lineDt->SetLineStyle(2);
    lineDt->Draw("SAME");

    // 4. Charge Spectrum in pC (60Co Compton Continuum & Edge)
    c1->cd(4);
    gPad->SetGrid();
    gPad->SetLogy();
    tree->Draw(Form("%s * %.10f>>hChargeRaw(150, 0, %f)", brChg.Data(), adc_to_pc, maxPcAxis), "", "HIST");
    TH1F* hChargeRaw = dynamic_cast<TH1F*>(gDirectory->Get("hChargeRaw"));
    if (hChargeRaw) {
        hChargeRaw->SetTitle("4. ^{60}Co Compton Charge Spectrum (Raw vs Amp Cut);Pulse Integral Charge Q (pC);Counts");
        hChargeRaw->SetLineColor(kGray + 2);
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

    TLegend* leg1 = new TLegend(0.35, 0.74, 0.68, 0.88);
    if (hChargeRaw) leg1->AddEntry(hChargeRaw, "Raw (THR = 30 ADC)", "f");
    if (hChargeCut) leg1->AddEntry(hChargeCut, Form("Amp > %.0f ADC", ampCut), "f");
    leg1->Draw();

    // 5. Amplitude vs Charge 2D Scatter
    c1->cd(5);
    gPad->SetGrid();
    gPad->SetLogz();
    gPad->SetRightMargin(0.13);
    tree->Draw(Form("%s:(%s * %.10f)>>h2D(150, 0, %f, 150, 0, %f)",
                    brAmp.Data(), brChg.Data(), adc_to_pc, maxPcAxis, maxAmpAxis), "", "colz");
    if (TH2F* h2D = dynamic_cast<TH2F*>(gDirectory->Get("h2D"))) {
        h2D->SetTitle("5. Amplitude vs Charge 2D Correlation;Pulse Integral Charge Q (pC);Amplitude (ADC)");
        h2D->SetStats(0);
    }

    // 6. Educational Hardware & Radioactive Source Physics Summary
    c1->cd(6);
    TPaveText* pt = new TPaveText(0.06, 0.08, 0.94, 0.92);
    pt->AddText("[NKFADC500 Mini Educational Summary - ^{60}Co #gamma Run]");
    pt->AddText(" ");
    pt->AddText(Form("- Digitizer: 12-bit (4096), %.1f Vpp (50 #Omega), %.0f MS/s (#Deltat = %.1f ns)", vpp, 1000.0 / dt_ns, dt_ns));
    pt->AddText(Form("- Calibration: 1 ADC = %.4f mV | 1 ADC*sample = %.5f pC", (vpp * 1000.0) / 4096.0, adc_to_pc));
    pt->AddText(Form("- Detector: Hamamatsu H7195 PMT (-1700 V) + EDA_CA Liquid Scintillator"));
    pt->AddText(Form("- Source: ^{60}Co (1.173 / 1.332 MeV #gamma, Compton Edge E_{CE} = 0.963 / 1.118 MeV)"));
    pt->AddText(Form("- Events: %lld evts | Peak Timing: t_{peak} = 372.0 ns (DLY = 400 ns)", tree->GetEntries()));
    pt->AddText(Form("- Cuts: Amp > %.1f ADC (Noise) | #Deltat > %.0f #mus (Afterpulse Check)", ampCut, dtCut_us));
    pt->SetFillColor(kWhite);
    pt->SetShadowColor(kWhite);
    pt->SetTextFont(42);
    pt->SetTextSize(0.038);
    pt->Draw();

    c1->Modified();
    c1->Update();
    if (savePng && std::string(savePng).size() > 0) {
        c1->SaveAs(savePng);
    }
}
