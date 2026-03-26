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
#include "TTreeReader.h"
#include "TTreeReaderArray.h"
#include "TTreeReaderValue.h"

// [교육용 분석 매크로]
// 실행 방법: root -l 'offline_anal_edu.cpp("data/run_101_prod.root", 0, 150.0, 100.0)'
// 파라미터: 파일명, 분석할 채널, Amplitude 컷(노이즈 제거), Delta T 컷(Afterpulse 제거, 단위 us)
void offline_anal_edu(const char* filename = "data/run_101_prod.root", int targetCh = 0, double ampCut = 150.0, double dtCut_us = 10.0) {
    gStyle->SetOptStat(111111);
    gStyle->SetPalette(kRainBow);

    TFile* f = TFile::Open(filename, "READ");
    if (!f || f->IsZombie()) {
        std::cout << "\033[1;31m[ERROR]\033[0m 파일 열기 실패: " << filename << std::endl;
        return;
    }
    TTree* tree = (TTree*)f->Get("PROD");
    if (!tree) return;

    TCanvas* c1 = new TCanvas("c1", "Educational Physics Analysis (Cut-based)", 1600, 1000);
    c1->Divide(3, 2); // 3x2 교육용 6분할 레이아웃

    // =========================================================================
    // [STEP 1] 기초 시각화: 파형 누적도 및 진폭(Amplitude) 분포
    // =========================================================================
    c1->cd(1); gPad->SetGrid(); gPad->SetRightMargin(0.12);
    tree->Draw(Form("wDrop_Ch%d:wTime_Ch%d>>hWave2D(512, 0, 1024, 200, -50, 3000)", targetCh, targetCh), "", "colz");
    TH2F* hWave2D = (TH2F*)gDirectory->Get("hWave2D");
    hWave2D->SetTitle("1. Waveform Persistence;Time (ns);Voltage Drop (ADC)"); hWave2D->SetStats(0);

    c1->cd(2); gPad->SetGrid(); gPad->SetLogy();
    tree->Draw(Form("Amplitude[%d]>>hAmp(500, 0, 4000)", targetCh), "", "HIST");
    TH1F* hAmp = (TH1F*)gDirectory->Get("hAmp");
    hAmp->SetTitle("2. Amplitude Distribution;Amplitude (ADC);Counts");
    hAmp->SetFillColor(kAzure-9);
    TLine* lineAmp = new TLine(ampCut, 0, ampCut, hAmp->GetMaximum());
    lineAmp->SetLineColor(kRed); lineAmp->SetLineWidth(2); lineAmp->SetLineStyle(2); lineAmp->Draw("SAME");

    // =========================================================================
    // [STEP 2] 고급 분석: TriggerTime 기반 Delta T 계산 (수동 루프)
    // =========================================================================
    c1->cd(3); gPad->SetGrid(); gPad->SetLogy();
    
    // 시간 차이(dt)를 저장할 커스텀 히스토그램 생성
    TH1F* hDeltaT = new TH1F("hDeltaT", "3. Time Between Events (#Delta t);#Delta t (#mu s);Counts", 500, 0, 1000);
    hDeltaT->SetFillColor(kTeal-9);

    TTreeReader reader("PROD", f);
    TTreeReaderValue<unsigned int> triggerTime(reader, "TriggerTime"); // 8ns resolution 클럭이라 가정

    unsigned int prevTime = 0;
    bool isFirst = true;

    while (reader.Next()) {
        unsigned int currTime = *triggerTime;
        if (!isFirst) {
            // 시간 차이 계산 (클럭 오버플로우 대비 및 8ns를 us로 변환)
            double dt_us = (currTime >= prevTime) ? (currTime - prevTime) * 0.008 : ((0xFFFFFFFF - prevTime) + currTime) * 0.008;
            hDeltaT->Fill(dt_us);
        }
        prevTime = currTime;
        isFirst = false;
    }
    hDeltaT->Draw("HIST");
    TLine* lineDt = new TLine(dtCut_us, 0, dtCut_us, hDeltaT->GetMaximum());
    lineDt->SetLineColor(kMagenta); lineDt->SetLineWidth(2); lineDt->SetLineStyle(2); lineDt->Draw("SAME");

    // =========================================================================
    // [STEP 3] 컷(Cut)의 위력 확인: 노이즈 필터링 전/후 비교
    // =========================================================================
    c1->cd(4); gPad->SetGrid(); gPad->SetLogy();
    tree->Draw(Form("Charge[%d]>>hChargeRaw(500, 0, 50000)", targetCh), "", "HIST");
    TH1F* hChargeRaw = (TH1F*)gDirectory->Get("hChargeRaw");
    hChargeRaw->SetTitle("4. Charge Spectrum (Raw vs Amp Cut);Charge (ADC sum);Counts");
    hChargeRaw->SetLineColor(kGray+1); hChargeRaw->SetFillColor(kGray+1); hChargeRaw->SetFillStyle(3004);

    TCut cutAmp = Form("Amplitude[%d] > %f", targetCh, ampCut);
    tree->Draw(Form("Charge[%d]>>hChargeCut(500, 0, 50000)", targetCh), cutAmp, "HIST SAME");
    TH1F* hChargeCut = (TH1F*)gDirectory->Get("hChargeCut");
    hChargeCut->SetLineColor(kRed+1); hChargeCut->SetFillColor(kRed-9); hChargeCut->SetFillStyle(1001);

    TLegend* leg1 = new TLegend(0.6, 0.75, 0.88, 0.88);
    leg1->AddEntry(hChargeRaw, "Raw (All)", "f"); leg1->AddEntry(hChargeCut, Form("Amp > %.0f", ampCut), "f"); leg1->Draw();

    // =========================================================================
    // [STEP 4] 멀티 컷 조합 & 산점도 (물리적 유의미성 검증)
    // =========================================================================
    c1->cd(5); gPad->SetGrid(); gPad->SetRightMargin(0.12);
    tree->Draw(Form("Amplitude[%d]:Charge[%d]>>h2D(200,0,50000, 200,0,4000)", targetCh, targetCh), "", "colz");
    TH2F* h2D = (TH2F*)gDirectory->Get("h2D");
    h2D->SetTitle("5. Amplitude vs Charge Scatter;Charge;Amplitude"); h2D->SetStats(0);

    c1->cd(6);
    // [교육 포인트] ROOT에 내장된 문서화 공간 활용
    TPaveText *pt = new TPaveText(0.1, 0.1, 0.9, 0.9);
    pt->AddText("🎓 [Educational Pipeline Summary]");
    pt->AddText(" ");
    pt->AddText(Form("- Target Channel : %d", targetCh));
    pt->AddText(Form("- Applied Amp Cut: > %.1f ADC (Removes Base Noise)", ampCut));
    pt->AddText(Form("- Applied Time Cut: > %.1f #mu s (Removes Afterpulses)", dtCut_us));
    pt->AddText(" ");
    pt->AddText("By combining Amplitude and #Delta t Cuts,");
    pt->AddText("we extract pure physics signals from harsh environments.");
    pt->SetFillColor(kWhite); pt->SetShadowColor(kWhite); pt->SetTextFont(42); pt->SetTextSize(0.045);
    pt->Draw();

    c1->Modified(); c1->Update();
}