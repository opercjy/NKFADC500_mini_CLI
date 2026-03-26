#include <iostream>
#include "TFile.h"
#include "TTree.h"
#include "TH1F.h"
#include "TF1.h"
#include "TCanvas.h"
#include "TStyle.h"
#include "TMath.h"
#include "TArrow.h"
#include "TLatex.h"
#include "Math/MinimizerOptions.h"

Double_t SmearedComptonEdge(Double_t *x, Double_t *par) {
    return par[0] * TMath::Erfc((x[0] - par[1]) / (TMath::Sqrt(2.0) * par[2])) + par[3];
}

// Usage: root -l 'offline_compton_edge.cpp("data/run_101_prod.root", 0, 2000.0)'
void offline_compton_edge(const char* filename = "data/run_101_prod.root", int targetCh = 0, double x_max_pc = 2000.0) {
    gStyle->SetOptFit(1111); 
    TFile* f = TFile::Open(filename, "READ");
    if (!f || f->IsZombie()) return;
    TTree* tree = (TTree*)f->Get("PROD");

    TCanvas* c1 = new TCanvas("c1_compton", "Compton Edge Solver", 1000, 700);
    gPad->SetGrid();

    double adc_to_pc = 0.048828125; 
    tree->Draw(Form("Charge[%d] * %f>>hCompton(400, 0, %f)", targetCh, adc_to_pc, x_max_pc), "", "HIST");
    TH1F* hCompton = (TH1F*)gDirectory->Get("hCompton");
    hCompton->SetTitle(Form("Compton Edge Spectrum (LS) - Ch %d;Charge (pC);Counts", targetCh));
    hCompton->SetLineColor(kAzure+2); hCompton->SetFillColor(kAzure+1); hCompton->SetFillStyle(3004);
    hCompton->SetMaximum(hCompton->GetMaximum() * 1.3);

    ROOT::Math::MinimizerOptions::SetDefaultMinimizer("Minuit2"); 

    // [Right-to-Left Edge Search Algorithm]
    int startBin = hCompton->FindBin(x_max_pc * 0.1); 
    int lastBin = hCompton->GetNbinsX();
    
    double continuumAmp = 0;
    for (int b = startBin; b <= lastBin; ++b) {
        if (hCompton->GetBinContent(b) > continuumAmp) continuumAmp = hCompton->GetBinContent(b);
    }
    
    double guessEdgePos = x_max_pc * 0.5;
    for (int b = lastBin; b >= startBin; --b) {
        if (hCompton->GetBinContent(b) >= continuumAmp * 0.5) {
            guessEdgePos = hCompton->GetBinCenter(b); break;
        }
    }

    TF1* fitEdge = new TF1("fitEdge", SmearedComptonEdge, guessEdgePos * 0.5, guessEdgePos * 1.3, 4);
    fitEdge->SetParNames("Amplitude", "Compton_Edge_pC", "Resolution_Sigma", "Background");
    fitEdge->SetLineColor(kRed); fitEdge->SetLineWidth(3);
    
    fitEdge->SetParameters(continuumAmp, guessEdgePos, guessEdgePos * 0.1, 10.0);
    fitEdge->SetParLimits(1, guessEdgePos * 0.7, guessEdgePos * 1.2); 

    hCompton->Fit("fitEdge", "QR+"); 

    double edge_pc = fitEdge->GetParameter(1);
    double y_edge = fitEdge->Eval(edge_pc);

    TArrow *arrEdge = new TArrow(edge_pc + (x_max_pc * 0.05), y_edge + (continuumAmp * 0.25), edge_pc, y_edge, 0.015, "|>");
    arrEdge->SetLineColor(kMagenta); arrEdge->SetLineWidth(2); arrEdge->SetFillColor(kMagenta); arrEdge->Draw();

    TLatex *latEdge = new TLatex(edge_pc + (x_max_pc * 0.06), y_edge + (continuumAmp * 0.25), "E_{Compton Edge}");
    latEdge->SetTextSize(0.035); latEdge->SetTextColor(kMagenta); latEdge->SetTextAlign(12); latEdge->Draw();

    std::cout << "\n========================================================" << std::endl;
    std::cout << " [Compton Edge Inverse Solution | Channel " << targetCh << "]" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << " 1. Compton Edge Position : \033[1;36m" << edge_pc << " pC\033[0m" << std::endl;
    std::cout << " 2. Detector Resolution   : " << fitEdge->GetParameter(2) << " pC" << std::endl;
    std::cout << "========================================================" << std::endl;
}