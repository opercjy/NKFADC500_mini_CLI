#include <iostream>
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TMath.h"
#include "TPaveText.h"
#include "Math/MinimizerOptions.h"

Double_t MultiGaussianFit(Double_t *x, Double_t *par) {
    Double_t val = x[0];
    Double_t ped = par[0] * TMath::Gaus(val, par[1], par[2]);
    Double_t spe = par[3] * TMath::Gaus(val, par[4], par[5]);
    Double_t dpe_mean = par[1] + 2.0 * (par[4] - par[1]); 
    Double_t dpe_sigma = par[5] * TMath::Sqrt(2.0);           
    Double_t dpe = par[6] * TMath::Gaus(val, dpe_mean, dpe_sigma);
    return ped + spe + dpe;
}

void offline_spe(const char* filename = "data/run_101_prod.root", int targetCh = 0) {
    gStyle->SetOptFit(1111); 
    
    TFile* f = TFile::Open(filename, "READ");
    if (!f || f->IsZombie()) return;
    TTree* tree = (TTree*)f->Get("PROD");

    double adc_to_pc = 0.048828125; 

    double max_adc = tree->GetMaximum(Form("Charge[%d]", targetCh));
    double max_pc = max_adc * adc_to_pc;
    double full_range_max = (max_pc > 1500.0) ? max_pc * 1.1 : 1500.0;

    TCanvas* c1 = new TCanvas("c1_spe", "Advanced PMT SPE Calibration (MINUIT2)", 1400, 600);
    c1->Divide(2, 1);

    // 1. Full Dynamic Range
    c1->cd(1);
    gPad->SetLogy(); gPad->SetGrid();
    tree->Draw(Form("Charge[%d] * %f>>hFull(500, -20.0, %f)", targetCh, adc_to_pc, full_range_max), "", "HIST");
    TH1F* hFull = (TH1F*)gDirectory->Get("hFull");
    hFull->SetTitle(Form("Full Dynamic Range Spectrum - Ch %d;Charge (pC);Counts", targetCh));
    hFull->SetLineColor(kTeal+2); hFull->SetFillColor(kTeal-9); hFull->SetFillStyle(3004);

    double mean_charge = hFull->GetMean();
    bool isScintillator = (mean_charge > 15.0 || max_pc > 150.0);

    TPaveText* pt = new TPaveText(0.35, 0.65, 0.88, 0.88, "NDC");
    if (isScintillator) {
        pt->AddText("Data Type: SCINTILLATOR / SOURCE");
        pt->AddText("High flux & broad spectrum detected.");
        pt->AddText("Please reduce flux (Laser/LED) for pure SPE.");
        pt->SetTextColor(kRed+1);
    } else {
        pt->AddText("Data Type: LASER / LED (SPE Mode)");
        pt->AddText("Low flux detected. Excellent for SPE fit.");
        pt->SetTextColor(kGreen+2);
    }
    pt->SetFillColor(kWhite); pt->SetTextAlign(12); pt->Draw();

    // 2. Zoomed SPE Fit Region
    c1->cd(2);
    gPad->SetLogy(); gPad->SetGrid();
    double fit_max_pc = 50.0; 
    
    tree->Draw(Form("Charge[%d] * %f>>hSPE(200, -5.0, %f)", targetCh, adc_to_pc, fit_max_pc), "", "HIST");
    TH1F* hSPE = (TH1F*)gDirectory->Get("hSPE");
    hSPE->SetTitle(Form("Low Charge Region (Pedestal + 1PE + 2PE Fit) - Ch %d;Charge (pC);Counts", targetCh));
    hSPE->SetLineColor(kAzure+1); hSPE->SetFillColor(kAzure+1); hSPE->SetFillStyle(3004);
    hSPE->SetMaximum(hSPE->GetMaximum() * 10.0);

    ROOT::Math::MinimizerOptions::SetDefaultMinimizer("Minuit2"); 

    double ped_peak = hSPE->GetMaximum();
    double ped_pos = hSPE->GetBinCenter(hSPE->GetMaximumBin());
    
    TF1* fitFunc = new TF1("fitFunc", MultiGaussianFit, -2.0, fit_max_pc, 7);
    fitFunc->SetParNames("Ped_Norm", "Ped_Mean", "Ped_Sigma", "1PE_Norm", "1PE_Mean", "1PE_Sigma", "2PE_Norm");
    fitFunc->SetLineColor(kMagenta); fitFunc->SetLineWidth(2);
    
    fitFunc->SetParameters(ped_peak, ped_pos, 0.5, ped_peak*0.05, ped_pos + 2.0, 1.0, ped_peak*0.005);
    fitFunc->SetParLimits(1, ped_pos - 1.0, ped_pos + 1.0);   
    fitFunc->SetParLimits(4, ped_pos + 0.5, fit_max_pc * 0.8); 

    std::cout << "\n[MINUIT2] Executing Pedestal + Multi-PE Fit..." << std::endl;
    hSPE->Fit("fitFunc", "QR+"); 

    TF1 *fPed = new TF1("fPed", "gaus", -5.0, fit_max_pc);
    fPed->SetParameters(fitFunc->GetParameter(0), fitFunc->GetParameter(1), fitFunc->GetParameter(2));
    fPed->SetLineColor(kBlack); fPed->SetLineStyle(2); fPed->Draw("SAME"); 

    TF1 *f1pe = new TF1("f1pe", "gaus", -5.0, fit_max_pc);
    f1pe->SetParameters(fitFunc->GetParameter(3), fitFunc->GetParameter(4), fitFunc->GetParameter(5));
    f1pe->SetLineColor(kRed); f1pe->SetLineStyle(2); f1pe->Draw("SAME");   

    TF1 *f2pe = new TF1("f2pe", "gaus", -5.0, fit_max_pc);
    f2pe->SetParameters(fitFunc->GetParameter(6), 
                        fitFunc->GetParameter(1) + 2.0 * (fitFunc->GetParameter(4) - fitFunc->GetParameter(1)), 
                        fitFunc->GetParameter(5) * TMath::Sqrt(2.0));
    f2pe->SetLineColor(kGreen+2); f2pe->SetLineStyle(2); f2pe->Draw("SAME"); 
    
    Double_t mean_ped = fitFunc->GetParameter(1);
    Double_t mean_1pe = fitFunc->GetParameter(4);
    Double_t q_pc = mean_1pe - mean_ped; 
    Double_t gain = q_pc * 1e-12 / 1.602e-19; 

    // 3. 터미널 콘솔 리포트
    std::cout << "\n========================================================" << std::endl;
    std::cout << " [ MINUIT2 SPE Calibration | Channel " << targetCh << " ]" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    
    if (isScintillator) {
        std::cout << "\033[1;31m [WARNING] Scintillator or Source data detected!\033[0m" << std::endl;
        std::cout << "\033[1;33m  -> High flux and broad distribution observed (Mean: " << mean_charge << " pC).\033[0m" << std::endl;
        std::cout << "\033[1;33m  -> Ah! This looks like Scintillator data.\033[0m" << std::endl;
        std::cout << "\033[1;33m  -> Please reduce the light flux (use Laser/LED mode) \033[0m" << std::endl;
        std::cout << "\033[1;33m     to accurately enter the Single-Photon measurement mode.\033[0m" << std::endl;
        std::cout << "--------------------------------------------------------" << std::endl;
        std::cout << " [ Attempted Fit Parameters (Rough Estimate) ]" << std::endl;
    } else {
        std::cout << "\033[1;32m [OK] Pure Laser/LED SPE data detected.\033[0m" << std::endl;
        std::cout << "--------------------------------------------------------" << std::endl;
    }

    std::cout << " 1. Pedestal Mean   : " << mean_ped << " pC" << std::endl;
    std::cout << " 2. 1-PE Peak Mean  : " << mean_1pe << " pC" << std::endl;
    std::cout << " 3. Pure 1-PE Charge: " << q_pc << " pC" << std::endl;
    std::cout << " 4. Absolute Gain   : \033[1;36m" << std::scientific << gain << "\033[0m" << std::fixed << std::endl;
    std::cout << "========================================================" << std::endl;

    c1->Modified();
    c1->Update();
}