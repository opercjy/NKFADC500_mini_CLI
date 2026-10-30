#include <iostream>
#include <string>
#include <vector>
#include "TCut.h"
#include "TCanvas.h"
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
} // namespace

void offline_anal_500(const char* filename = "data/run_101_prod.root", int targetCh = 0, double ampCut = 50.0) {
    if (targetCh < 0 || targetCh > 3) {
        std::cout << "\033[1;31m[ERROR]\033[0m targetCh must be in [0, 3] (got " << targetCh << ")." << std::endl;
        return;
    }

    gStyle->SetOptStat(111111);
    gStyle->SetPalette(kBird);

    TFile* f = TFile::Open(filename, "READ");
    if (!f || f->IsZombie()) {
        std::cout << "\033[1;31m[ERROR]\033[0m Cannot open file: " << filename << std::endl;
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

    TCanvas* c1 = new TCanvas("c1", Form("Channel %d Comprehensive Analysis", targetCh), 1400, 900);
    c1->Divide(2, 2);

    // 1. Waveform Persistence (if -w saved) or 2D Charge-Comparison PSD fallback
    c1->cd(1);
    gPad->SetGrid();
    gPad->SetRightMargin(0.12);
    if (tree->GetBranch(brWaveDrop) && tree->GetBranch(brWaveTime)) {
        tree->Draw(Form("%s:%s>>hWave2D(%d, 0, %f, 200, -100, 4200)",
                        brWaveDrop.Data(), brWaveTime.Data(), ndp, maxTime), "", "colz");
        if (TH2F* hWave2D = dynamic_cast<TH2F*>(gDirectory->Get("hWave2D"))) {
            hWave2D->SetTitle(Form("All Waveforms Persistence (Ch %d);Time (ns);Voltage Drop (ADC)", targetCh));
            hWave2D->SetStats(0);
        }
    } else if (tree->GetBranch(brPsd) && tree->GetBranch(brQtot)) {
        tree->Draw(Form("%s:%s>>hPsd2D(200, 0, 10000, 120, 0.0, 1.0)",
                        brPsd.Data(), brQtot.Data()), "", "colz");
        if (TH2F* hPsd2D = dynamic_cast<TH2F*>(gDirectory->Get("hPsd2D"))) {
            hPsd2D->SetTitle(Form("Charge-Comparison PSD (Ch %d, Fast Mode);Q_{total} (ADC*sample);Q_{tail} / Q_{total}", targetCh));
            hPsd2D->SetStats(0);
        }
    } else {
        TPaveText* pt = new TPaveText(0.1, 0.3, 0.9, 0.7, "NDC");
        pt->AddText("Waveforms omitted in Fast Physics Mode.");
        pt->AddText("Re-run production_nkfadc_500 with -w for 2D persistence.");
        pt->SetFillColor(kWhite);
        pt->Draw();
    }

    // 2. Amplitude Distribution & Threshold Cut Guideline
    c1->cd(2);
    gPad->SetGrid();
    gPad->SetLogy();
    tree->Draw(Form("%s>>hAmp(500, 0, 4200)", brAmp.Data()), "", "HIST");
    if (TH1F* hAmp = dynamic_cast<TH1F*>(gDirectory->Get("hAmp"))) {
        hAmp->SetTitle(Form("Amplitude Distribution (Ch %d);Amplitude (ADC);Counts", targetCh));
        hAmp->SetLineColor(kAzure + 1);
        hAmp->SetFillColor(kAzure - 9);
        hAmp->SetFillStyle(1001);

        TLine* cutLine = new TLine(ampCut, 0.5, ampCut, std::max(1.0, hAmp->GetMaximum() * 2.0));
        cutLine->SetLineColor(kRed);
        cutLine->SetLineWidth(2);
        cutLine->SetLineStyle(2);
        cutLine->Draw("SAME");
    }

    // 3. Charge Spectrum (Raw vs Amplitude Cut)
    c1->cd(3);
    gPad->SetGrid();
    gPad->SetLogy();
    tree->Draw(Form("%s>>hChargeRaw(500, 0, 50000)", brChg.Data()), "", "HIST");
    TH1F* hChargeRaw = dynamic_cast<TH1F*>(gDirectory->Get("hChargeRaw"));
    if (hChargeRaw) {
        hChargeRaw->SetTitle(Form("Charge Spectrum: Raw vs Cut (Amp > %.1f);Charge (ADC*sample);Counts", ampCut));
        hChargeRaw->SetLineColor(kGray + 1);
        hChargeRaw->SetFillColor(kGray + 1);
        hChargeRaw->SetFillStyle(3004);
    }

    TCut physicsCut = Form("%s > %f", brAmp.Data(), ampCut);
    tree->Draw(Form("%s>>hChargeCut(500, 0, 50000)", brChg.Data()), physicsCut, "HIST SAME");
    TH1F* hChargeCut = dynamic_cast<TH1F*>(gDirectory->Get("hChargeCut"));
    if (hChargeCut) {
        hChargeCut->SetLineColor(kRed + 1);
        hChargeCut->SetFillColor(kRed - 9);
        hChargeCut->SetFillStyle(1001);
    }

    TLegend* leg = new TLegend(0.60, 0.75, 0.88, 0.88);
    if (hChargeRaw) leg->AddEntry(hChargeRaw, "Raw (All Events)", "f");
    if (hChargeCut) leg->AddEntry(hChargeCut, Form("Cut (Amp > %.0f ADC)", ampCut), "f");
    leg->Draw();

    // 4. Amplitude vs Charge 2D Correlation
    c1->cd(4);
    gPad->SetGrid();
    gPad->SetRightMargin(0.12);
    tree->Draw(Form("%s:%s>>h2D(200, 0, 50000, 200, -100, 4200)", brAmp.Data(), brChg.Data()), "", "colz");
    if (TH2F* h2D = dynamic_cast<TH2F*>(gDirectory->Get("h2D"))) {
        h2D->SetTitle(Form("Amplitude vs Charge Scatter (Ch %d);Charge (ADC*sample);Amplitude (ADC)", targetCh));
        h2D->SetStats(0);
    }

    c1->Modified();
    c1->Update();
}
