#include <iostream>
#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TCut.h"
#include "TLine.h"
#include "TLegend.h"
#include "TStyle.h"
#include "TPaveText.h"
#include "TTreeReader.h"
#include "TTreeReaderValue.h"

// Usage: root -l 'offline_anal_edu.cpp("data/run_101_prod.root", 0, 50.0, 10.0)'
void offline_anal_edu(const char* filename = "data/run_101_prod.root", int targetCh = 0, double ampCut = 50.0, double dtCut_us = 10.0) {
    gStyle->SetOptStat(111111);
    gStyle->SetPalette(kBird);

    TFile* f = TFile::Open(filename, "READ");
    if (!f || f->IsZombie()) {
        std::cerr << "\033[1;31m[ERROR]\033[0m Cannot open file: " << filename << std::endl;
        return;
    }
    TTree* tree = (TTree*)f->Get("PROD");
    if (!tree) return;

    // 💡 [Dynamic Scaling] Read NDP (RecordLength) to set X-axis dynamically
    int ndp = 512;
    tree->SetBranchAddress("RecordLength", &ndp);
    tree->GetEntry(0);
    double maxTime = ndp * 2.0; // 500MS/s = 2.0 ns per point

    TCanvas* c1 = new TCanvas("c1", "NKFADC500 Educational Analysis Dashboard", 1600, 1000);
    c1->Divide(3, 2);

    // =========================================================================
    // 1. Waveform Persistence
    // =========================================================================
    c1->cd(1); gPad->SetGrid(); gPad->SetRightMargin(0.12);
    // Y-axis dynamically scaled up to 12-bit max (4096)
    tree->Draw(Form("wDrop_Ch%d:wTime_Ch%d>>hWave2D(%d, 0, %f, 200, -100, 4200)", targetCh, targetCh, ndp, maxTime), "", "colz");
    TH2F* hWave2D = (TH2F*)gDirectory->Get("hWave2D");
    if(hWave2D) { hWave2D->SetTitle(Form("1. Waveform Persistence (Ch %d);Time (ns);Voltage Drop (ADC)", targetCh)); hWave2D->SetStats(0); }

    // =========================================================================
    // 2. Amplitude Distribution
    // =========================================================================
    c1->cd(2); gPad->SetGrid(); gPad->SetLogy();
    tree->Draw(Form("Amplitude[%d]>>hAmp(500, 0, 4200)", targetCh), "", "HIST");
    TH1F* hAmp = (TH1F*)gDirectory->Get("hAmp");
    hAmp->SetTitle("2. Amplitude Distribution;Amplitude (ADC);Counts");
    hAmp->SetFillColor(kAzure-9);
    TLine* lineAmp = new TLine(ampCut, 0, ampCut, hAmp->GetMaximum());
    lineAmp->SetLineColor(kRed); lineAmp->SetLineWidth(2); lineAmp->SetLineStyle(2); lineAmp->Draw("SAME");

    // =========================================================================
    // 3. Delta T (Time between events)
    // =========================================================================
    c1->cd(3); gPad->SetGrid(); gPad->SetLogy();
    TH1F* hDeltaT = new TH1F("hDeltaT", "3. Time Between Events (#Delta t);#Delta t (#mu s);Counts", 500, 0, 1000);
    hDeltaT->SetFillColor(kTeal-9);

    TTreeReader reader("PROD", f);
    TTreeReaderValue<ULong64_t> triggerTime(reader, "TriggerTime"); // FADC500 uses ns resolution clock

    ULong64_t prevTime = 0;
    bool isFirst = true;

    while (reader.Next()) {
        ULong64_t currTime = *triggerTime;
        if (!isFirst) {
            double dt_us = (currTime - prevTime) / 1000.0; // ns -> us
            hDeltaT->Fill(dt_us);
        }
        prevTime = currTime;
        isFirst = false;
    }
    hDeltaT->Draw("HIST");
    TLine* lineDt = new TLine(dtCut_us, 0, dtCut_us, hDeltaT->GetMaximum());
    lineDt->SetLineColor(kMagenta); lineDt->SetLineWidth(2); lineDt->SetLineStyle(2); lineDt->Draw("SAME");

    // =========================================================================
    // 4. Charge Spectrum (Physical Unit: pC)
    // =========================================================================
    c1->cd(4); gPad->SetGrid(); gPad->SetLogy();
    double adc_to_pc = 0.048828125; // 5.0V / 4096 / 50Ohm * 2.0ns * 1000
    
    tree->Draw(Form("Charge[%d] * %f>>hChargeRaw(500, 0, 1000)", targetCh, adc_to_pc), "", "HIST");
    TH1F* hChargeRaw = (TH1F*)gDirectory->Get("hChargeRaw");
    hChargeRaw->SetTitle("4. Charge Spectrum (Raw vs Amp Cut);Charge (pC);Counts");
    hChargeRaw->SetLineColor(kGray+1); hChargeRaw->SetFillColor(kGray+1); hChargeRaw->SetFillStyle(3004);

    TCut cutAmp = Form("Amplitude[%d] > %f", targetCh, ampCut);
    tree->Draw(Form("Charge[%d] * %f>>hChargeCut(500, 0, 1000)", targetCh, adc_to_pc), cutAmp, "HIST SAME");
    TH1F* hChargeCut = (TH1F*)gDirectory->Get("hChargeCut");
    hChargeCut->SetLineColor(kRed+1); hChargeCut->SetFillColor(kRed-9); hChargeCut->SetFillStyle(1001);

    TLegend* leg1 = new TLegend(0.6, 0.75, 0.88, 0.88);
    leg1->AddEntry(hChargeRaw, "Raw (All)", "f"); leg1->AddEntry(hChargeCut, Form("Amp > %.0f", ampCut), "f"); leg1->Draw();

    // =========================================================================
    // 5. Amplitude vs Charge 2D Scatter
    // =========================================================================
    c1->cd(5); gPad->SetGrid(); gPad->SetRightMargin(0.12);
    tree->Draw(Form("Amplitude[%d]:(Charge[%d] * %f)>>h2D(200,0,1000, 200,0,4200)", targetCh, targetCh, adc_to_pc), "", "colz");
    TH2F* h2D = (TH2F*)gDirectory->Get("h2D");
    if(h2D) { h2D->SetTitle("5. Amp vs Charge Scatter;Charge (pC);Amplitude (ADC)"); h2D->SetStats(0); }

    // =========================================================================
    // 6. Educational Summary (Pure English)
    // =========================================================================
    c1->cd(6);
    TPaveText *pt = new TPaveText(0.1, 0.1, 0.9, 0.9);
    pt->AddText("[NKFADC500 Educational Summary]");
    pt->AddText(" ");
    pt->AddText(Form("- Hardware: 12-bit (4096), +/- 2.5V (5Vpp), 500MS/s"));
    pt->AddText(Form("- 1 ADC Sum = %.4f pC", adc_to_pc));
    pt->AddText(Form("- Target Channel : %d", targetCh));
    pt->AddText(Form("- Amp Cut: > %.1f ADC (Removes Base Noise)", ampCut));
    pt->AddText(Form("- Time Cut: > %.1f us (Removes Afterpulses)", dtCut_us));
    pt->SetFillColor(kWhite); pt->SetShadowColor(kWhite); pt->SetTextFont(42); pt->SetTextSize(0.045);
    pt->Draw();

    c1->Modified(); c1->Update();
}