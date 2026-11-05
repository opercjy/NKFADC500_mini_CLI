#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include "TCanvas.h"
#include "TCut.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TLegend.h"
#include "TLine.h"
#include "TPaveText.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TTree.h"

namespace {
TString ResolveAnalBranch(TTree* tree, const char* base, int ch) {
    TString scalarName = Form("%s_Ch%d", base, ch);
    if (tree->GetBranch(scalarName)) return scalarName;
    return Form("%s[%d]", base, ch);
}

TString ResolveDefaultProdFile(const char* requested) {
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

void offline_anal_500(const char* filename = "data/20261104_160_temp_EDA_CA_1000_ml_per_l_60co_hv_1700_thr_30_min_20_prod.root",
                      int targetCh = 0,
                      double ampCut = 45.0,
                      const char* savePng = "") {
    if (targetCh < 0 || targetCh > 3) {
        std::cout << "\033[1;31m[ERROR]\033[0m targetCh must be in [0, 3] (got " << targetCh << ")." << std::endl;
        return;
    }

    gStyle->SetOptStat(111111);
    gStyle->SetPalette(kBird);
    if (!gSystem->AccessPathName("lib/libFADC500Objects.so")) {
        gSystem->Load("lib/libFADC500Objects.so");
    }

    const TString resolvedFile = ResolveDefaultProdFile(filename);
    TFile* f = TFile::Open(resolvedFile.Data(), "READ");
    if (!f || f->IsZombie()) {
        std::cout << "\033[1;31m[ERROR]\033[0m Cannot open file: " << resolvedFile << std::endl;
        return;
    }

    TTree* tree = dynamic_cast<TTree*>(f->Get("PROD"));
    if (!tree || tree->GetEntries() == 0) {
        std::cout << "\033[1;31m[ERROR]\033[0m PROD tree not found or empty!" << std::endl;
        return;
    }

    int ndp = 496;
    double dt_ns = 2.0;
    if (tree->GetBranch("RecordLength")) {
        tree->SetBranchAddress("RecordLength", &ndp);
    }
    if (tree->GetBranch("SamplingNs")) {
        tree->SetBranchAddress("SamplingNs", &dt_ns);
    }
    tree->GetEntry(0);
    tree->ResetBranchAddresses();
    if (ndp <= 0) ndp = 496;
    if (dt_ns <= 0.0) dt_ns = 2.0;
    const double maxTime = ndp * dt_ns;

    const TString brAmp = ResolveAnalBranch(tree, "Amplitude", targetCh);
    const TString brChg = ResolveAnalBranch(tree, "Charge", targetCh);
    const TString brWaveDrop = Form("wDrop_Ch%d", targetCh);
    const TString brWaveTime = Form("wTime_Ch%d", targetCh);
    const TString brPsd = Form("PsdRatio_Ch%d", targetCh);
    const TString brQtot = Form("Qtotal_Ch%d", targetCh);

    const double maxAmpAxis = 350.0;
    const double maxChgAxis = 1800.0; // 1800 ADC*sample (~44 pC) covers 60Co Compton continuum & edge

    TCanvas* c1 = new TCanvas("c1", Form("Channel %d Comprehensive 60Co Source Analysis", targetCh), 1450, 920);
    c1->Divide(2, 2);

    // 1. Waveform Persistence (if -w saved) or 2D Charge-Comparison PSD fallback
    c1->cd(1);
    gPad->SetGrid();
    gPad->SetLogz();
    gPad->SetRightMargin(0.13);
    if (tree->GetBranch(brWaveDrop) && tree->GetBranch(brWaveTime)) {
        tree->Draw(Form("%s:%s>>hWave2D(%d, 0, %f, 220, -25, %f)",
                        brWaveDrop.Data(), brWaveTime.Data(), ndp, maxTime, maxAmpAxis),
                   "Entry$ < 25000", "colz");
        if (TH2F* hWave2D = dynamic_cast<TH2F*>(gDirectory->Get("hWave2D"))) {
            hWave2D->SetTitle(Form("1. 2D Waveform Persistence (Ch %d, ^{60}Co #gamma Source);Time (ns);Voltage Drop (ADC)", targetCh));
            hWave2D->SetStats(0);
        }
    } else if (tree->GetBranch(brPsd) && tree->GetBranch(brQtot)) {
        tree->Draw(Form("%s:%s>>hPsd2D(200, 0, %f, 120, 0.0, 1.0)",
                        brPsd.Data(), brQtot.Data(), maxChgAxis), "", "colz");
        if (TH2F* hPsd2D = dynamic_cast<TH2F*>(gDirectory->Get("hPsd2D"))) {
            hPsd2D->SetTitle(Form("1. Charge-Comparison PSD (Ch %d);Q_{total} (ADC*sample);Q_{tail} / Q_{total}", targetCh));
            hPsd2D->SetStats(0);
        }
    }

    // 2. Amplitude Distribution & Threshold Cut Guideline
    c1->cd(2);
    gPad->SetGrid();
    gPad->SetLogy();
    tree->Draw(Form("%s>>hAmp(160, 0, %f)", brAmp.Data(), maxAmpAxis), "", "HIST");
    if (TH1F* hAmp = dynamic_cast<TH1F*>(gDirectory->Get("hAmp"))) {
        hAmp->SetTitle(Form("2. Pulse Amplitude Distribution (Ch %d, ^{60}Co);Amplitude (ADC) [1 ADC = 0.610 mV];Counts", targetCh));
        hAmp->SetLineColor(kAzure + 2);
        hAmp->SetFillColor(kAzure - 9);
        hAmp->SetFillStyle(1001);

        TLine* cutLine = new TLine(ampCut, 0.5, ampCut, std::max(1.0, hAmp->GetMaximum() * 1.6));
        cutLine->SetLineColor(kRed + 1);
        cutLine->SetLineWidth(2);
        cutLine->SetLineStyle(2);
        cutLine->Draw("SAME");
    }

    // 3. Charge Spectrum (Raw vs Amplitude Cut)
    c1->cd(3);
    gPad->SetGrid();
    gPad->SetLogy();
    tree->Draw(Form("%s>>hChargeRaw(160, 0, %f)", brChg.Data(), maxChgAxis), "", "HIST");
    TH1F* hChargeRaw = dynamic_cast<TH1F*>(gDirectory->Get("hChargeRaw"));
    if (hChargeRaw) {
        hChargeRaw->SetTitle(Form("3. ^{60}Co Compton Charge Spectrum (Raw vs Amp > %.0f ADC);Integral Charge (ADC*sample) [1 ADC*s = 0.0244 pC];Counts", ampCut));
        hChargeRaw->SetLineColor(kGray + 2);
        hChargeRaw->SetFillColor(kGray + 1);
        hChargeRaw->SetFillStyle(3004);
    }

    TCut physicsCut = Form("%s > %f", brAmp.Data(), ampCut);
    tree->Draw(Form("%s>>hChargeCut(160, 0, %f)", brChg.Data(), maxChgAxis), physicsCut, "HIST SAME");
    TH1F* hChargeCut = dynamic_cast<TH1F*>(gDirectory->Get("hChargeCut"));
    if (hChargeCut) {
        hChargeCut->SetLineColor(kRed + 1);
        hChargeCut->SetFillColor(kRed - 9);
        hChargeCut->SetFillStyle(1001);
    }

    TLegend* leg = new TLegend(0.38, 0.74, 0.72, 0.88);
    if (hChargeRaw) leg->AddEntry(hChargeRaw, "All Triggered Events (THR = 30 ADC)", "f");
    if (hChargeCut) leg->AddEntry(hChargeCut, Form("Selected Physics Cut (Amp > %.0f ADC)", ampCut), "f");
    leg->Draw();

    // 4. Amplitude vs Charge 2D Correlation
    c1->cd(4);
    gPad->SetGrid();
    gPad->SetLogz();
    gPad->SetRightMargin(0.13);
    tree->Draw(Form("%s:%s>>h2D(160, 0, %f, 160, 0, %f)", brAmp.Data(), brChg.Data(), maxChgAxis, maxAmpAxis), "", "colz");
    if (TH2F* h2D = dynamic_cast<TH2F*>(gDirectory->Get("h2D"))) {
        h2D->SetTitle(Form("4. Amplitude vs Integral Charge Correlation (Ch %d);Integral Charge (ADC*sample);Amplitude (ADC)", targetCh));
        h2D->SetStats(0);
    }

    c1->Modified();
    c1->Update();
    if (savePng && std::string(savePng).size() > 0) {
        c1->SaveAs(savePng);
    }
}
