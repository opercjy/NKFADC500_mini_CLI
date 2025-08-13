#include <iostream>
#include <vector>
#include <string>

#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TH2F.h"
#include "TGraph.h"
#include "TStyle.h"
#include "TApplication.h"
#include "TAxis.h"

// 이 스크립트의 메인 함수입니다.
void visualize_waveforms(const char* filename) {
    // --- 1. 파일 및 TTree 열기 ---
    TFile *file = TFile::Open(filename, "READ");
    if (!file || file->IsZombie()) {
        std::cerr << "Error: Cannot open file '" << filename << "'" << std::endl;
        return;
    }
    TTree *tree = dynamic_cast<TTree*>(file->Get("fadc_tree"));
    if (!tree) {
        std::cerr << "Error: Cannot find 'fadc_tree' in the file." << std::endl;
        file->Close();
        return;
    }

    Long64_t n_entries = tree->GetEntries();
    std::cout << "File '" << filename << "' opened successfully." << std::endl;
    std::cout << "Found " << n_entries << " events in 'fadc_tree'." << std::endl;

    // --- 2. TTree 브랜치와 C++ 변수 연결 ---
    std::vector<uint16_t> *wf[4] = {nullptr};
    tree->SetBranchAddress("waveform1", &wf[0]);
    tree->SetBranchAddress("waveform2", &wf[1]);
    tree->SetBranchAddress("waveform3", &wf[2]);
    tree->SetBranchAddress("waveform4", &wf[3]);
    
    // 첫 번째 이벤트로 파형 샘플 수 확인
    tree->GetEntry(0);
    if (!wf[0] || wf[0]->empty()) {
        std::cerr << "Error: Waveform data is empty or invalid." << std::endl;
        file->Close();
        return;
    }
    const int n_samples = wf[0]->size();
    const double time_step = 2.0; // ns, TODO: run_info에서 읽어오도록 개선 가능
    const double time_range = n_samples * time_step;

    // --- 3. 전체 이벤트 누적 파형 그리기 ---
    TCanvas *c_cumulative = new TCanvas("c_cumulative", "Cumulative Waveforms (All Events)", 1200, 800);
    c_cumulative->Divide(2, 2);

    TH2F *h_cumul[4];
    for (int ch = 0; ch < 4; ++ch) {
        h_cumul[ch] = new TH2F(Form("h_cumul_ch%d", ch + 1),
                               Form("Cumulative Waveform Ch %d;Time (ns);ADC Counts", ch + 1),
                               n_samples, 0, time_range,
                               256, 0, 4096);
    }

    std::cout << "\nGenerating cumulative plots... This may take a moment." << std::endl;
    for (Long64_t i = 0; i < n_entries; ++i) {
        tree->GetEntry(i);
        for (int ch = 0; ch < 4; ++ch) {
            for (int s = 0; s < wf[ch]->size(); ++s) {
                h_cumul[ch]->Fill(s * time_step, wf[ch]->at(s));
            }
        }
    }
    std::cout << "Done." << std::endl;

    for (int ch = 0; ch < 4; ++ch) {
        c_cumulative->cd(ch + 1)->SetGrid();
        h_cumul[ch]->Draw("COLZ");
    }
    c_cumulative->Update();

    // --- 4. 개별 이벤트 대화형 뷰어 ---
    TCanvas *c_event = new TCanvas("c_event", "Event Viewer", 1200, 800);
    c_event->Divide(2, 2);

    std::vector<double> time_axis(n_samples);
    for (int i = 0; i < n_samples; ++i) time_axis[i] = i * time_step;

    Long64_t current_entry = 0;
    std::string command;

    while (true) {
        if (current_entry < 0) current_entry = 0;
        if (current_entry >= n_entries) current_entry = n_entries - 1;

        tree->GetEntry(current_entry);

        for (int ch = 0; ch < 4; ++ch) {
            c_event->cd(ch + 1)->SetGrid();
            std::vector<double> wf_double(wf[ch]->begin(), wf[ch]->end());
            TGraph *g = new TGraph(n_samples, time_axis.data(), wf_double.data());
            g->SetTitle(Form("Event %lld - Channel %d;Time (ns);ADC Counts", current_entry, ch + 1));
            g->SetLineColor(kBlue + 2);
            g->SetLineWidth(2);
            g->GetYaxis()->SetRangeUser(0, 4096);
            g->Draw("AL");
        }
        c_event->Update();

        std::cout << "\n[이벤트 " << current_entry << "/" << n_entries - 1 << "] n: 다음, p: 이전, j [번호]: 점프, q: 종료 > ";
        std::cin >> command;

        if (command == "n") current_entry++;
        else if (command == "p") current_entry--;
        else if (command == "j") { std::cin >> current_entry; }
        else if (command == "q") break;
        else { std::cout << "잘못된 입력입니다." << std::endl; }
    }

    std::cout << "Exiting visualization script." << std::endl;
    file->Close();
}

#if !defined(__CLING__)
int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input_root_file>" << std::endl;
        return 1;
    }
    TApplication app("App", &argc, argv);
    visualize_waveforms(argv[1]);
    std::cout << "\nAll ROOT windows closed. Exiting application." << std::endl;
    return 0;
}
#endif
