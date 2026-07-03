#include <iostream>
#include <vector>
#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TCut.h"
#include "TLine.h"
#include "TLegend.h"
#include "TStyle.h"
#include "TSystem.h"

void offline_anal_500(const char* filename = "data/run_101_prod.root", int targetCh = 0, double ampCut = 50.0) {
    gStyle->SetOptStat(111111);
    gStyle->SetPalette(kBird); 

    TFile* f = TFile::Open(filename, "READ");
    if (!f || f->IsZombie()) {
        std::cout << "\033[1;31m[ERROR]\033[0m Cannot open file: " << filename << std::endl;
        return;
    }

    TTree* tree = (TTree*)f->Get("PROD");
    if (!tree) {
        std::cout << "\033[1;31m[ERROR]\033[0m PROD tree not found!" << std::endl;
        return;
    }

    // [동적 스케일링] RecordLength 기반 자동 시간축 설정
    int ndp = 512;
    if (tree->GetBranch("RecordLength")) {
        tree->SetBranchAddress("RecordLength", &ndp);
        tree->GetEntry(0);
    }
    double maxTime = ndp * 2.0;

    TCanvas* c1 = new TCanvas("c1", Form("Channel %d Comprehensive Analysis", targetCh), 1400, 900);
    c1->Divide(2, 2);

    // 1. 파형 누적 밀도도 (Waveform Persistence)
    c1->cd(1);
    gPad->SetGrid(); gPad->SetRightMargin(0.12);
    tree->Draw(Form("wDrop_Ch%d:wTime_Ch%d>>hWave2D(%d, 0, %f, 200, -100, 4200)", targetCh, targetCh, ndp, maxTime), "", "colz");
    TH2F* hWave2D = (TH2F*)gDirectory->Get("hWave2D");
    if (hWave2D) {
        hWave2D->SetTitle(Form("All Waveforms Persistence (Ch %d);Time (ns);Voltage Drop (ADC)", targetCh));
        hWave2D->SetStats(0); 
    }

    // 2. 진폭 분포 및 컷(Cut) 가이드라인
    c1->cd(2);
    gPad->SetGrid(); gPad->SetLogy();
    tree->Draw(Form("Amplitude[%d]>>hAmp(500, 0, 4200)", targetCh), "", "HIST");
    TH1F* hAmp = (TH1F*)gDirectory->Get("hAmp");
    if (hAmp) {
        hAmp->SetTitle(Form("Amplitude Distribution (Ch %d);Amplitude (ADC);Counts", targetCh));
        hAmp->SetLineColor(kAzure+1); hAmp->SetFillColor(kAzure-9); hAmp->SetFillStyle(1001);
        
        TLine* cutLine = new TLine(ampCut, 0, ampCut, hAmp->GetMaximum() * 2);
        cutLine->SetLineColor(kRed); cutLine->SetLineWidth(2); cutLine->SetLineStyle(2);
        cutLine->Draw("SAME");
    }

    // 3. 전하량 스펙트럼 (Raw vs Cut 비교)
    c1->cd(3);
    gPad->SetGrid(); gPad->SetLogy();
    
    tree->Draw(Form("Charge[%d]>>hChargeRaw(500, 0, 50000)", targetCh), "", "HIST");
    TH1F* hChargeRaw = (TH1F*)gDirectory->Get("hChargeRaw");
    hChargeRaw->SetTitle(Form("Charge Spectrum: Raw vs Cut (Amp > %.1f);Charge (ADC sum);Counts", ampCut));
    hChargeRaw->SetLineColor(kGray+1); hChargeRaw->SetFillColor(kGray+1); hChargeRaw->SetFillStyle(3004);

    TCut physicsCut = Form("Amplitude[%d] > %f", targetCh, ampCut);
    tree->Draw(Form("Charge[%d]>>hChargeCut(500, 0, 50000)", targetCh), physicsCut, "HIST SAME");
    TH1F* hChargeCut = (TH1F*)gDirectory->Get("hChargeCut");
    hChargeCut->SetLineColor(kRed+1); hChargeCut->SetFillColor(kRed-9); hChargeCut->SetFillStyle(1001);

    TLegend* leg = new TLegend(0.65, 0.75, 0.88, 0.88);
    leg->AddEntry(hChargeRaw, "Raw (All Events)", "f");
    leg->AddEntry(hChargeCut, Form("Cut (Amp > %.0f)", ampCut), "f");
    leg->Draw();

    // 4. 진폭 vs 전하량 2D 상관관계 산점도
    c1->cd(4);
    gPad->SetGrid(); gPad->SetRightMargin(0.12);
    tree->Draw(Form("Amplitude[%d]:Charge[%d]>>h2D(200,0,50000, 200,-100,4200)", targetCh, targetCh), "", "colz");
    TH2F* h2D = (TH2F*)gDirectory->Get("h2D");
    if (h2D) {
        h2D->SetTitle(Form("Amplitude vs Charge Scatter (Ch %d);Charge (ADC sum);Amplitude (ADC)", targetCh));
        h2D->SetStats(0);
    }

    c1->Modified();
    c1->Update();
}