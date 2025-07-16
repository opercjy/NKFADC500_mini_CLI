/**
 * @file view_waveform_pro.C
 * @brief FADC TTree 파일을 분석하는 전문가용 시각화 스크립트
 * @details
 * - 실행 정보를 읽어와 정확한 시간 축 계산
 * - 모든 이벤트의 파형을 겹쳐 그린 2D 누적 히스토그램 표시
 * - 이전/다음/점프/종료 등 상세한 이벤트 네비게이션 기능 제공
 *
 * @usage
 * // 1. ROOT 실행: root -l
 * // 2. 스크립트 로드: root [0] .L view_waveform_pro.C
 * // 3. 함수 실행: root [1] view_waveform("my_run_data.root")
 */

#include <iostream>
#include <vector>
#include <string>

#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TSystem.h"
#include "TStyle.h"
#include "TLatex.h"

// --- 전역 변수 ---
TFile* g_infile = nullptr;
TTree* g_event_tree = nullptr;
Long64_t g_n_entries = 0;
Long64_t g_current_entry = -1;

// --- 실행 정보 ---
ULong_t g_sr_mode = 0;
ULong_t g_rl_mode = 0;
double g_time_per_sample = 2.0; // ns, 기본값

// --- ROOT 객체 ---
TCanvas* g_canvas_event = nullptr;
TCanvas* g_canvas_cumulative = nullptr;
TH1F* g_h_wf[4] = {nullptr};
TH2F* g_h_cumulative[4] = {nullptr};

// 함수 프로토타입
void show_event(Long64_t entry);
void show_cumulative();

// 메인 함수
void view_waveform(const std::string& root_filename) {
    g_infile = TFile::Open(root_filename.c_str(), "READ");
    if (!g_infile || g_infile->IsZombie()) {
        std::cerr << "Error: Cannot open input file '" << root_filename << "'" << std::endl;
        return;
    }

    // 1. 실행 정보 읽기
    TTree* run_info_tree = dynamic_cast<TTree*>(g_infile->Get("run_info"));
    if (run_info_tree) {
        run_info_tree->SetBranchAddress("sampling_rate_mode", &g_sr_mode);
        run_info_tree->GetEntry(0);
        g_time_per_sample = 2.0 * pow(2, g_sr_mode);
        std::cout << "Run Info Loaded: Sampling Rate Mode = " << g_sr_mode 
                  << " -> Time per sample = " << g_time_per_sample << " ns" << std::endl;
    } else {
        std::cout << "Warning: 'run_info' TTree not found. Using default time scale (2 ns/sample)." << std::endl;
    }

    // 2. 이벤트 TTree 가져오기
    g_event_tree = dynamic_cast<TTree*>(g_infile->Get("fadc_tree"));
    if (!g_event_tree) {
        std::cerr << "Error: Cannot find TTree 'fadc_tree' in the file." << std::endl;
        g_infile->Close();
        return;
    }
    g_n_entries = g_event_tree->GetEntries();
    std::cout << "Total entries in TTree: " << g_n_entries << std::endl;

    // 3. 사용자 메뉴 제공
    std::string command;
    while (true) {
        std::cout << "\n[메뉴] e: 이벤트 보기 | c: 누적 플롯 보기 | q: 종료 > ";
        std::cin >> command;
        if (command == "e") {
            show_event(0);
        } else if (command == "c") {
            show_cumulative();
        } else if (command == "q") {
            break;
        } else {
            std::cout << "잘못된 입력입니다." << std::endl;
        }
    }

    g_infile->Close();
    std::cout << "Exiting waveform viewer." << std::endl;
}

// 개별 이벤트 표시 함수
void show_event(Long64_t entry) {
    if (!g_canvas_event) {
        g_canvas_event = new TCanvas("c_event", "Event Display", 1200, 800);
    }
    g_canvas_event->cd();
    
    std::vector<uint16_t>* wf[4] = {nullptr};
    g_event_tree->SetBranchAddress("waveform1", &wf[0]);
    g_event_tree->SetBranchAddress("waveform2", &wf[1]);
    g_event_tree->SetBranchAddress("waveform3", &wf[2]);
    g_event_tree->SetBranchAddress("waveform4", &wf[3]);

    g_current_entry = entry;

    std::string command;
    while (true) {
        if (g_current_entry < 0) g_current_entry = 0;
        if (g_current_entry >= g_n_entries) g_current_entry = g_n_entries - 1;

        g_event_tree->GetEntry(g_current_entry);
        
        if (!g_h_wf[0]) {
            int n_samples = wf[0]->size();
            float time_range = n_samples * g_time_per_sample;
            for(int i=0; i<4; ++i) {
                g_h_wf[i] = new TH1F(Form("h_wf%d",i+1), "", n_samples, 0, time_range);
                g_h_wf[i]->GetXaxis()->SetTitle("Time (ns)");
                g_h_wf[i]->GetYaxis()->SetTitle("ADC Counts");
            }
        }
        
        g_canvas_event->Clear();
        g_canvas_event->Divide(2, 2);
        for(int i=0; i<4; ++i) {
            g_h_wf[i]->Reset();
            for(size_t j=0; j<wf[i]->size(); ++j) g_h_wf[i]->SetBinContent(j+1, wf[i]->at(j));
            g_canvas_event->cd(i+1)->SetGrid();
            g_h_wf[i]->SetTitle(Form("Event %lld - Channel %d", g_current_entry, i+1));
            g_h_wf[i]->Draw("HIST L");
        }
        g_canvas_event->Update();

        std::cout << "[이벤트 " << g_current_entry << "/" << g_n_entries-1 << "] n: 다음, p: 이전, j [번호]: 점프, q: 메뉴로 > ";
        std::cin >> command;

        if (command == "n") g_current_entry++;
        else if (command == "p") g_current_entry--;
        else if (command == "j") {
            Long64_t jump_to;
            std::cin >> jump_to;
            g_current_entry = jump_to;
        }
        else if (command == "q") break;
    }
}

// 누적 플롯 표시 함수
void show_cumulative() {
    if (!g_canvas_cumulative) {
        g_canvas_cumulative = new TCanvas("c_cumulative", "Cumulative Waveform Display", 1200, 800);
        g_canvas_cumulative->Divide(2, 2);

        std::vector<uint16_t>* wf_ptr = nullptr;
        g_event_tree->SetBranchAddress("waveform1", &wf_ptr);
        g_event_tree->GetEntry(0);
        int n_samples = wf_ptr->size();
        float time_range = n_samples * g_time_per_sample;

        for(int i=0; i<4; ++i) {
            g_h_cumulative[i] = new TH2F(Form("h_cumul%d", i+1), Form("Cumulative Ch %d;Time (ns);ADC Counts", i+1),
                                         n_samples, 0, time_range, 512, 0, 4096);
        }

        std::cout << "Generating cumulative plots for all " << g_n_entries << " events... please wait." << std::endl;
        std::vector<uint16_t>* wfs[4] = {nullptr};
        g_event_tree->SetBranchAddress("waveform1", &wfs[0]);
        g_event_tree->SetBranchAddress("waveform2", &wfs[1]);
        g_event_tree->SetBranchAddress("waveform3", &wfs[2]);
        g_event_tree->SetBranchAddress("waveform4", &wfs[3]);

        for (Long64_t i = 0; i < g_n_entries; ++i) {
            g_event_tree->GetEntry(i);
            for (int ch=0; ch<4; ++ch) {
                for (size_t s=0; s<wfs[ch]->size(); ++s) {
                    g_h_cumulative[ch]->Fill(s * g_time_per_sample, wfs[ch]->at(s));
                }
            }
        }
        std::cout << "Done." << std::endl;
    }

    g_canvas_cumulative->cd();
    for(int i=0; i<4; ++i) {
        g_canvas_cumulative->cd(i+1)->SetGrid();
        g_h_cumulative[i]->Draw("COLZ");
    }
    g_canvas_cumulative->Update();
    std::cout << "누적 플롯이 표시되었습니다. 캔버스를 닫으면 메뉴로 돌아갑니다." << std::endl;
    g_canvas_cumulative->WaitPrimitive();
}
