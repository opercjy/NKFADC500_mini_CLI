#include <algorithm>
#include <iostream>
#include "Math/MinimizerOptions.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TMath.h"
#include "TPaveText.h"
#include "TString.h"
#include "TStyle.h"
#include "TTree.h"

namespace {
TString ResolveSpeBranch(TTree* tree, const char* base, int ch) {
    TString scalarName = Form("%s_Ch%d", base, ch);
    if (tree->GetBranch(scalarName)) return scalarName;
    return Form("%s[%d]", base, ch);
}
} // namespace

Double_t MultiGaussianFit(Double_t* x, Double_t* par) {
    const Double_t val = x[0];
    const Double_t ped_sig = std::max(1e-5, par[2]);
    const Double_t spe_sig = std::max(1e-5, par[5]);
    const Double_t ped = par[0] * TMath::Gaus(val, par[1], ped_sig);
    const Double_t spe = par[3] * TMath::Gaus(val, par[4], spe_sig);
    const Double_t dpe_mean = par[1] + 2.0 * (par[4] - par[1]);
    const Double_t dpe_sigma = spe_sig * TMath::Sqrt(2.0);
    const Double_t dpe = par[6] * TMath::Gaus(val, dpe_mean, dpe_sigma);
    return ped + spe + dpe;
}

// NOTICE NKFADC500 Mini nominal input: 2.5 Vpp into 50 Ohm at 500 MS/s (2.0 ns/sample)
// -> 1 ADC*sample = (2.5 V / 4096) * (2.0 ns / 50 Ohm) = 0.0244140625 pC
void offline_spe(const char* filename = "data/run_101_prod.root", int targetCh = 0, double vpp = 2.5) {
    if (targetCh < 0 || targetCh > 3) {
        std::cerr << "\033[1;31m[ERROR]\033[0m targetCh must be in [0, 3]." << std::endl;
        return;
    }

    gStyle->SetOptFit(1111);

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

    double dt_ns = 2.0;
    if (tree->GetBranch("SamplingNs")) {
        tree->SetBranchAddress("SamplingNs", &dt_ns);
        tree->GetEntry(0);
        tree->ResetBranchAddresses();
    }
    if (dt_ns <= 0.0) dt_ns = 2.0;

    const double adc_to_pc = (vpp / 4096.0) * (dt_ns / 50.0) * 1000.0;
    const TString brChg = ResolveSpeBranch(tree, "Charge", targetCh);

    const double max_adc = tree->GetMaximum(brChg.Data());
    const double max_pc = max_adc * adc_to_pc;
    const double full_range_max = (max_pc > 750.0) ? max_pc * 1.1 : 750.0;

    TCanvas* c1 = new TCanvas("c1_spe", "Advanced PMT SPE Calibration (MINUIT2)", 1400, 600);
    c1->Divide(2, 1);

    // 1. Full Dynamic Range
    c1->cd(1);
    gPad->SetLogy();
    gPad->SetGrid();
    tree->Draw(Form("%s * %.10f>>hFull(500, -10.0, %f)", brChg.Data(), adc_to_pc, full_range_max), "", "HIST");
    TH1F* hFull = dynamic_cast<TH1F*>(gDirectory->Get("hFull"));
    if (!hFull) return;
    hFull->SetTitle(Form("Full Dynamic Range Spectrum - Ch %d;Charge (pC);Counts", targetCh));
    hFull->SetLineColor(kTeal + 2);
    hFull->SetFillColor(kTeal - 9);
    hFull->SetFillStyle(3004);

    const double mean_charge = hFull->GetMean();
    const bool isScintillator = (mean_charge > 10.0 || max_pc > 80.0);

    TPaveText* pt = new TPaveText(0.35, 0.65, 0.88, 0.88, "NDC");
    if (isScintillator) {
        pt->AddText("Data Type: SCINTILLATOR / SOURCE");
        pt->AddText("High flux & broad spectrum detected.");
        pt->AddText("Reduce optical flux (Laser/LED) for pure SPE.");
        pt->SetTextColor(kRed + 1);
    } else {
        pt->AddText("Data Type: LASER / LED (SPE Mode)");
        pt->AddText("Low flux detected. Suitable for SPE fit.");
        pt->SetTextColor(kGreen + 2);
    }
    pt->SetFillColor(kWhite);
    pt->SetTextAlign(12);
    pt->Draw();

    // 2. Zoomed SPE Fit Region
    c1->cd(2);
    gPad->SetLogy();
    gPad->SetGrid();
    const double fit_max_pc = 25.0;

    tree->Draw(Form("%s * %.10f>>hSPE(200, -2.5, %f)", brChg.Data(), adc_to_pc, fit_max_pc), "", "HIST");
    TH1F* hSPE = dynamic_cast<TH1F*>(gDirectory->Get("hSPE"));
    if (!hSPE) return;
    hSPE->SetTitle(Form("Low Charge Region (Pedestal + 1PE + 2PE Fit) - Ch %d;Charge (pC);Counts", targetCh));
    hSPE->SetLineColor(kAzure + 1);
    hSPE->SetFillColor(kAzure + 1);
    hSPE->SetFillStyle(3004);
    hSPE->SetMaximum(std::max(10.0, hSPE->GetMaximum() * 10.0));

    ROOT::Math::MinimizerOptions::SetDefaultMinimizer("Minuit2");

    const double ped_peak = std::max(1.0, hSPE->GetMaximum());
    const double ped_pos = hSPE->GetBinCenter(hSPE->GetMaximumBin());

    TF1* fitFunc = new TF1("fitFunc", MultiGaussianFit, -1.0, fit_max_pc, 7);
    fitFunc->SetParNames("Ped_Norm", "Ped_Mean", "Ped_Sigma", "1PE_Norm", "1PE_Mean", "1PE_Sigma", "2PE_Norm");
    fitFunc->SetLineColor(kMagenta);
    fitFunc->SetLineWidth(2);

    fitFunc->SetParameters(ped_peak, ped_pos, 0.25, ped_peak * 0.08, ped_pos + 0.55, 0.25, ped_peak * 0.01);
    fitFunc->SetParLimits(1, ped_pos - 0.5, ped_pos + 0.5);
    fitFunc->SetParLimits(2, 0.02, 2.0);
    fitFunc->SetParLimits(4, ped_pos + 0.15, fit_max_pc * 0.8);
    fitFunc->SetParLimits(5, 0.05, 5.0);

    std::cout << "\n[MINUIT2] Executing Pedestal + Multi-PE Fit..." << std::endl;
    hSPE->Fit("fitFunc", "QR+");

    TF1* fPed = new TF1("fPed", "gaus", -2.5, fit_max_pc);
    fPed->SetParameters(fitFunc->GetParameter(0), fitFunc->GetParameter(1), fitFunc->GetParameter(2));
    fPed->SetLineColor(kBlack);
    fPed->SetLineStyle(2);
    fPed->Draw("SAME");

    TF1* f1pe = new TF1("f1pe", "gaus", -2.5, fit_max_pc);
    f1pe->SetParameters(fitFunc->GetParameter(3), fitFunc->GetParameter(4), fitFunc->GetParameter(5));
    f1pe->SetLineColor(kRed);
    f1pe->SetLineStyle(2);
    f1pe->Draw("SAME");

    TF1* f2pe = new TF1("f2pe", "gaus", -2.5, fit_max_pc);
    f2pe->SetParameters(fitFunc->GetParameter(6),
                        fitFunc->GetParameter(1) + 2.0 * (fitFunc->GetParameter(4) - fitFunc->GetParameter(1)),
                        fitFunc->GetParameter(5) * TMath::Sqrt(2.0));
    f2pe->SetLineColor(kGreen + 2);
    f2pe->SetLineStyle(2);
    f2pe->Draw("SAME");

    const Double_t mean_ped = fitFunc->GetParameter(1);
    const Double_t mean_1pe = fitFunc->GetParameter(4);
    const Double_t q_pc = mean_1pe - mean_ped;
    const Double_t gain = (q_pc * 1e-12) / 1.602176634e-19;

    std::cout << "\n========================================================" << std::endl;
    std::cout << " [ MINUIT2 SPE Calibration | Channel " << targetCh << " ]" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    if (isScintillator) {
        std::cout << "\033[1;31m [WARNING] Scintillator or Source data detected!\033[0m" << std::endl;
        std::cout << "\033[1;33m  -> Broad charge spectrum observed (Mean: " << mean_charge << " pC).\033[0m" << std::endl;
        std::cout << "\033[1;33m  -> Use pulsed LED/Laser low-occupancy mode for exact SPE calibration.\033[0m" << std::endl;
        std::cout << "--------------------------------------------------------" << std::endl;
    } else {
        std::cout << "\033[1;32m [OK] Pure Laser/LED SPE data detected.\033[0m" << std::endl;
        std::cout << "--------------------------------------------------------" << std::endl;
    }

    std::cout << " 0. Calibration   : " << adc_to_pc << " pC / (ADC*sample) (" << vpp << " Vpp)" << std::endl;
    std::cout << " 1. Pedestal Mean : " << mean_ped << " pC" << std::endl;
    std::cout << " 2. 1-PE Peak Mean: " << mean_1pe << " pC" << std::endl;
    std::cout << " 3. Net 1-PE Q    : " << q_pc << " pC (" << (q_pc / adc_to_pc) << " ADC*sample)" << std::endl;
    std::cout << " 4. Absolute Gain : \033[1;36m" << std::scientific << gain << "\033[0m" << std::fixed << std::endl;
    std::cout << "========================================================" << std::endl;

    c1->Modified();
    c1->Update();
}
