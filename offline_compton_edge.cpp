#include <algorithm>
#include <iostream>
#include "Math/MinimizerOptions.h"
#include "TArrow.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TLatex.h"
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
} // namespace

Double_t SmearedComptonEdge(Double_t* x, Double_t* par) {
    const Double_t sigma = std::max(1e-6, par[2]);
    return par[0] * TMath::Erfc((x[0] - par[1]) / (TMath::Sqrt(2.0) * sigma)) + par[3];
}

// NOTICE NKFADC500 Mini nominal input: 2.5 Vpp into 50 Ohm at 500 MS/s (2.0 ns/sample)
// -> 1 ADC*sample = (2.5 V / 4096) * (2.0 ns / 50 Ohm) = 0.0244140625 pC
void offline_compton_edge(const char* filename = "data/run_101_prod.root",
                          int targetCh = 0,
                          double x_max_pc = 200.0,
                          double vpp = 2.5) {
    if (targetCh < 0 || targetCh > 3) {
        std::cerr << "\033[1;31m[ERROR]\033[0m targetCh must be in [0, 3]." << std::endl;
        return;
    }

    gStyle->SetOptFit(1111);
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

    double dt_ns = 2.0;
    if (tree->GetBranch("SamplingNs")) {
        tree->SetBranchAddress("SamplingNs", &dt_ns);
        tree->GetEntry(0);
        tree->ResetBranchAddresses();
    }
    if (dt_ns <= 0.0) dt_ns = 2.0;

    const double adc_to_pc = (vpp / 4096.0) * (dt_ns / 50.0) * 1000.0;
    const TString brChg = ResolveComptonBranch(tree, "Charge", targetCh);
    const double obs_max_pc = tree->GetMaximum(brChg.Data()) * adc_to_pc;
    if (obs_max_pc > 5.0 && obs_max_pc < x_max_pc * 0.75) {
        x_max_pc = std::max(30.0, obs_max_pc * 1.15);
    }

    TCanvas* c1 = new TCanvas("c1_compton", "Compton Edge Solver", 1000, 700);
    gPad->SetGrid();

    tree->Draw(Form("%s * %.10f>>hCompton(60, 0, %f)", brChg.Data(), adc_to_pc, x_max_pc), "", "HIST");
    TH1F* hCompton = dynamic_cast<TH1F*>(gDirectory->Get("hCompton"));
    if (!hCompton || hCompton->GetEntries() == 0) {
        std::cerr << "\033[1;31m[ERROR]\033[0m Failed to build Compton histogram for " << brChg << std::endl;
        return;
    }

    hCompton->SetTitle(Form("Compton Edge Spectrum (LS) - Ch %d;Charge (pC);Counts", targetCh));
    hCompton->SetLineColor(kAzure + 2);
    hCompton->SetFillColor(kAzure + 1);
    hCompton->SetFillStyle(3004);
    hCompton->SetMaximum(std::max(10.0, hCompton->GetMaximum() * 1.3));

    ROOT::Math::MinimizerOptions::SetDefaultMinimizer("Minuit2");

    const int peakBin = hCompton->GetMaximumBin();
    const double peakPos = hCompton->GetBinCenter(peakBin);
    const double continuumAmp = std::max(1.0, hCompton->GetMaximum());
    const int lastBin = hCompton->GetNbinsX();

    double guessEdgePos = peakPos * 1.18;
    for (int b = lastBin; b >= peakBin; --b) {
        if (hCompton->GetBinContent(b) >= continuumAmp * 0.45) {
            guessEdgePos = hCompton->GetBinCenter(b);
            break;
        }
    }

    TF1* fitEdge = new TF1("fitEdge", SmearedComptonEdge,
                           std::max(0.0, peakPos * 0.92),
                           std::min(x_max_pc, guessEdgePos * 1.45), 4);
    fitEdge->SetNpx(500);
    fitEdge->SetParNames("Amplitude", "Compton_Edge_pC", "Resolution_Sigma", "Background");
    fitEdge->SetLineColor(kRed + 1);
    fitEdge->SetLineWidth(3);

    fitEdge->SetParameters(continuumAmp * 0.48, guessEdgePos, std::max(1.5, guessEdgePos * 0.12), 0.5);
    fitEdge->SetParLimits(0, continuumAmp * 0.20, continuumAmp * 0.90);
    fitEdge->SetParLimits(1, peakPos * 1.02, std::min(x_max_pc, guessEdgePos * 1.30));
    fitEdge->SetParLimits(2, 1.2, x_max_pc * 0.30);
    fitEdge->SetParLimits(3, 0.0, continuumAmp * 0.25);

    hCompton->Fit("fitEdge", "QR+");
    fitEdge->Draw("SAME");

    const double edge_pc = fitEdge->GetParameter(1);
    const double y_edge = fitEdge->Eval(edge_pc);

    TArrow* arrEdge = new TArrow(edge_pc + (x_max_pc * 0.04), y_edge + (continuumAmp * 0.14), edge_pc, y_edge, 0.015, "|>");
    arrEdge->SetLineColor(kMagenta + 1);
    arrEdge->SetLineWidth(2);
    arrEdge->SetFillColor(kMagenta + 1);
    arrEdge->Draw();

    TLatex* latEdge = new TLatex(edge_pc + (x_max_pc * 0.045), y_edge + (continuumAmp * 0.14), "E_{Compton Edge}");
    latEdge->SetTextSize(0.033);
    latEdge->SetTextColor(kMagenta + 1);
    latEdge->SetTextAlign(12);
    latEdge->Draw();

    std::cout << "\n========================================================" << std::endl;
    std::cout << " [Compton Edge Inverse Solution | Channel " << targetCh << "]" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << " 0. Calibration Factor    : " << adc_to_pc << " pC / (ADC*sample) (" << vpp << " Vpp)" << std::endl;
    std::cout << " 1. Compton Edge Position : \033[1;36m" << edge_pc << " pC\033[0m (" << (edge_pc / adc_to_pc) << " ADC*sample)" << std::endl;
    std::cout << " 2. Detector Resolution   : " << fitEdge->GetParameter(2) << " pC" << std::endl;
    std::cout << "========================================================" << std::endl;

    c1->Modified();
    c1->Update();
}
