#include <algorithm>
#include <iostream>
#include <string>
#include "Math/MinimizerOptions.h"
#include "TArrow.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TLatex.h"
#include "TLegend.h"
#include "TLine.h"
#include "TMath.h"
#include "TString.h"
#include "TStyle.h"
#include "TSystem.h"
#include "TTree.h"

namespace {
TString ResolveComptonBranch(TTree* tree, const char* base, int ch) {
    TString scalarName = Form("%s_Ch%d", base, ch);
    if (tree->GetBranch(scalarName)) return scalarName;
    return Form("%s[%d]", base, ch);
}

TString ResolveDefaultComptonFile(const char* requested) {
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

Double_t SmearedComptonEdge(Double_t* x, Double_t* par) {
    const Double_t sigma = std::max(1e-6, par[2]);
    return par[0] * TMath::Erfc((x[0] - par[1]) / (TMath::Sqrt(2.0) * sigma)) + par[3];
}

// NOTICE NKFADC500 Mini nominal input: 2.5 Vpp into 50 Ohm at 500 MS/s (2.0 ns/sample)
// -> 1 ADC*sample = (2.5 V / 4096) * (2.0 ns / 50 Ohm) = 0.0244140625 pC
// For 60Co (1.173 & 1.332 MeV gamma), effective mean Compton edge E_CE = 1.041 MeVee (963 & 1118 keV).
void offline_compton_edge(const char* filename = "data/20261104_160_temp_EDA_CA_1000_ml_per_l_60co_hv_1700_thr_30_min_20_prod.root",
                          int targetCh = 0,
                          double x_max_pc = 38.0,
                          double vpp = 2.5,
                          const char* savePng = "") {
    if (targetCh < 0 || targetCh > 3) {
        std::cerr << "\033[1;31m[ERROR]\033[0m targetCh must be in [0, 3]." << std::endl;
        return;
    }

    gStyle->SetOptFit(1111);
    gStyle->SetOptStat(1110);
    if (!gSystem->AccessPathName("lib/libFADC500Objects.so")) {
        gSystem->Load("lib/libFADC500Objects.so");
    }

    const TString resolvedFile = ResolveDefaultComptonFile(filename);
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

    double dt_ns = 2.0;
    if (tree->GetBranch("SamplingNs")) {
        tree->SetBranchAddress("SamplingNs", &dt_ns);
        tree->GetEntry(0);
        tree->ResetBranchAddresses();
    }
    if (dt_ns <= 0.0) dt_ns = 2.0;

    const double adc_to_pc = (vpp / 4096.0) * (dt_ns / 50.0) * 1000.0;
    const TString brChg = ResolveComptonBranch(tree, "Charge", targetCh);

    TCanvas* c1 = new TCanvas("c1_compton", "60Co Compton Edge Solver (NKFADC500 Mini)", 1100, 760);
    gPad->SetGrid();

    tree->Draw(Form("%s * %.10f>>hCompton(76, 0, %f)", brChg.Data(), adc_to_pc, x_max_pc), "", "HIST");
    TH1F* hCompton = dynamic_cast<TH1F*>(gDirectory->Get("hCompton"));
    if (!hCompton || hCompton->GetEntries() == 0) {
        std::cerr << "\033[1;31m[ERROR]\033[0m Failed to build Compton histogram for " << brChg << std::endl;
        return;
    }

    hCompton->SetTitle(Form("^{60}Co Compton Continuum & Edge Fit (LS, Ch %d);Pulse Integral Charge Q (pC);Counts / 0.5 pC", targetCh));
    hCompton->SetLineColor(kAzure + 2);
    hCompton->SetLineWidth(2);
    hCompton->SetFillColor(kAzure - 9);
    hCompton->SetFillStyle(3004);
    hCompton->SetMaximum(std::max(10.0, hCompton->GetMaximum() * 1.28));

    ROOT::Math::MinimizerOptions::SetDefaultMinimizer("Minuit2");

    // Locate Compton continuum ridge peak in [6.0, 16.0] pC
    int peakBin = hCompton->GetMaximumBin();
    for (int b = hCompton->FindBin(6.0); b <= hCompton->FindBin(16.0); ++b) {
        if (hCompton->GetBinContent(b) > hCompton->GetBinContent(peakBin)) {
            peakBin = b;
        }
    }
    const double peakPos = hCompton->GetBinCenter(peakBin);
    const double continuumAmp = std::max(1.0, hCompton->GetBinContent(peakBin));
    const int lastBin = hCompton->GetNbinsX();

    double guessEdgePos = peakPos * 1.42;
    for (int b = peakBin + 1; b <= lastBin; ++b) {
        if (hCompton->GetBinContent(b) <= continuumAmp * 0.50) {
            guessEdgePos = hCompton->GetBinCenter(b);
            break;
        }
    }

    const double fitMin = std::max(8.5, peakPos + 0.5);
    const double fitMax = std::min(x_max_pc - 1.0, guessEdgePos * 1.95);

    TF1* fitEdge = new TF1("fitEdge", SmearedComptonEdge, fitMin, fitMax, 4);
    fitEdge->SetNpx(500);
    fitEdge->SetParNames("Amplitude", "Compton_Edge_pC", "Resolution_Sigma", "Background");
    fitEdge->SetLineColor(kRed + 1);
    fitEdge->SetLineWidth(3);

    fitEdge->SetParameters(continuumAmp * 0.52, guessEdgePos, std::max(1.8, guessEdgePos * 0.22), continuumAmp * 0.01);
    fitEdge->SetParLimits(0, continuumAmp * 0.20, continuumAmp * 0.95);
    fitEdge->SetParLimits(1, peakPos + 1.0, std::min(x_max_pc, guessEdgePos * 1.45));
    fitEdge->SetParLimits(2, 1.0, x_max_pc * 0.35);
    fitEdge->SetParLimits(3, 0.0, continuumAmp * 0.15);

    hCompton->Fit("fitEdge", "QR+");
    fitEdge->Draw("SAME");

    const double edge_pc = fitEdge->GetParameter(1);
    const double sigma_pc = fitEdge->GetParameter(2);
    const double y_edge = fitEdge->Eval(edge_pc);

    // Ridge vertical line
    TLine* lRidge = new TLine(peakPos, 0.0, peakPos, continuumAmp * 1.05);
    lRidge->SetLineColor(kGreen + 2);
    lRidge->SetLineStyle(2);
    lRidge->SetLineWidth(2);
    lRidge->Draw("SAME");

    // Compton edge arrow & annotation
    TArrow* arrEdge = new TArrow(edge_pc + (x_max_pc * 0.08), y_edge + (continuumAmp * 0.18), edge_pc, y_edge, 0.016, "|>");
    arrEdge->SetLineColor(kMagenta + 2);
    arrEdge->SetLineWidth(2);
    arrEdge->SetFillColor(kMagenta + 2);
    arrEdge->Draw();

    TLatex* latEdge = new TLatex(
        edge_pc + (x_max_pc * 0.085),
        y_edge + (continuumAmp * 0.18),
        Form("^{60}Co Compton Edge: Q_{CE} = %.2f #pm %.2f pC (#sigma = %.2f pC)", edge_pc, fitEdge->GetParError(1), sigma_pc)
    );
    latEdge->SetTextSize(0.030);
    latEdge->SetTextColor(kMagenta + 2);
    latEdge->SetTextAlign(12);
    latEdge->Draw();

    TLegend* leg = new TLegend(0.14, 0.74, 0.56, 0.88);
    leg->AddEntry(hCompton, Form("Measured ^{60}Co Charge Spectrum (N = %lld)", tree->GetEntries()), "f");
    leg->AddEntry(lRidge, Form("Compton Continuum Ridge (Q_{ridge} = %.1f pC)", peakPos), "l");
    leg->AddEntry(fitEdge, Form("Gaussian-Smeared Erfc Fit (Q_{CE} = %.2f pC)", edge_pc), "l");
    leg->Draw();

    std::cout << "\n========================================================" << std::endl;
    std::cout << " [60Co Compton Edge Inverse Solution | Channel " << targetCh << "]" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << " 0. Calibration Factor    : " << adc_to_pc << " pC / (ADC*sample) (" << vpp << " Vpp)" << std::endl;
    std::cout << " 1. Compton Continuum Peak: " << peakPos << " pC" << std::endl;
    std::cout << " 2. Compton Edge Position : \033[1;36m" << edge_pc << " pC\033[0m (" << (edge_pc / adc_to_pc) << " ADC*sample)" << std::endl;
    std::cout << " 3. Detector Resolution   : " << sigma_pc << " pC (" << (100.0 * sigma_pc / edge_pc) << " %)" << std::endl;
    std::cout << "========================================================" << std::endl;

    c1->Modified();
    c1->Update();
    if (savePng && std::string(savePng).size() > 0) {
        c1->SaveAs(savePng);
    }
}
