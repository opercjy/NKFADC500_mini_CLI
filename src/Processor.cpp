#include "Processor.h"
#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>
#include <cmath>

#include "TFile.h"
#include "TTree.h"
#include "TCanvas.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TSystem.h"
#include "TStyle.h"
#include "TGraph.h"
#include "TBox.h"
#include "TLatex.h"
#include "TLine.h"
#include "TPad.h"

// --- Processor 클래스 생성자 및 소멸자 ---

Processor::Processor(const std::string& filename) 
    : m_infilename(filename), m_infile(nullptr), m_event_tree(nullptr), 
      m_n_entries(0), m_current_entry(0), m_canvas_event(nullptr), m_canvas_cumulative(nullptr)
{
    // ROOT 객체 포인터 초기화
    for(int i=0; i<4; ++i) {
        m_h_wf[i] = nullptr;
        m_h_cumulative[i] = nullptr;
        m_h_charge[i] = nullptr;
        m_h_pulse_height[i] = nullptr;
        m_h_pulse_time[i] = nullptr;
        m_h_height_vs_time[i] = nullptr;
    }

    m_infile = TFile::Open(m_infilename.c_str(), "READ");
    if (m_infile && !m_infile->IsZombie()) {
        m_event_tree = dynamic_cast<TTree*>(m_infile->Get("fadc_tree"));
        if (m_event_tree) {
            m_n_entries = m_event_tree->GetEntries();
        }
        loadRunInfo();
    }
}

Processor::~Processor() {
    if (m_infile) m_infile->Close();
    // 생성된 ROOT 객체들을 수동으로 삭제
    delete m_canvas_event;
    delete m_canvas_cumulative;
    for(int i=0; i<4; ++i) {
        delete m_h_wf[i];
        delete m_h_cumulative[i];
        delete m_h_charge[i];
        delete m_h_pulse_height[i];
        delete m_h_pulse_time[i];
        delete m_h_height_vs_time[i];
    }
}

bool Processor::isValid() const {
    return (m_infile && !m_infile->IsZombie() && m_event_tree);
}

// --- private 헬퍼 함수 ---

bool Processor::loadRunInfo() {
    TTree* run_info_tree = dynamic_cast<TTree*>(m_infile->Get("run_info"));
    if (run_info_tree) {
        ULong64_t sr_value = 1; 
        run_info_tree->SetBranchAddress("polarity", &m_params.polarity);
        run_info_tree->SetBranchAddress("sampling_rate", &sr_value);
        run_info_tree->SetBranchAddress("recording_length", &m_params.recording_length);
        run_info_tree->GetEntry(0);
        m_params.time_per_sample = 2.0 * sr_value;
        std::cout << "Info: Run Info Loaded. Polarity=" << m_params.polarity 
                  << ", Sampling Rate Divisor=" << sr_value
                  << ", Time/Sample=" << m_params.time_per_sample << " ns" << std::endl;
        return true;
    }
    std::cout << "Warning: 'run_info' TTree not found. Using default parameters." << std::endl;
    return false;
}

// --- public 인터페이스 함수 ---

void Processor::processAndWrite() {
    if (!isValid() || m_event_tree->GetBranch("waveform1") == nullptr) {
        std::cerr << "Error: Input file must be a raw data file containing waveforms." << std::endl;
        return;
    }

    std::string outfilename = m_infilename;
    std::string suffix = ".prod";
    if (outfilename.length() < suffix.length() || 
        outfilename.substr(outfilename.length() - suffix.length()) != suffix) 
    {
        outfilename += suffix; // ".prod"로 끝나지 않으면 추가합니다.
    }
   

    TFile* outfile = TFile::Open(outfilename.c_str(), "RECREATE");
    TTree* new_tree = new TTree("fadc_tree", "Processed FADC500 Data");

    UInt_t event_num = 0, trigger_number = 0, trigger_pattern = 0;
    UChar_t trigger_type = 0;
    ULong64_t ttime = 0, ltime = 0;
    m_event_tree->SetBranchAddress("event", &event_num);
    m_event_tree->SetBranchAddress("trigger_number", &trigger_number);
    m_event_tree->SetBranchAddress("trigger_type", &trigger_type);
    m_event_tree->SetBranchAddress("trigger_pattern", &trigger_pattern);
    m_event_tree->SetBranchAddress("ttime", &ttime);
    m_event_tree->SetBranchAddress("ltime", &ltime);
    
    new_tree->Branch("event", &event_num);
    new_tree->Branch("trigger_number", &trigger_number);
    new_tree->Branch("trigger_type", &trigger_type);
    new_tree->Branch("trigger_pattern", &trigger_pattern);
    new_tree->Branch("ttime", &ttime);
    new_tree->Branch("ltime", &ltime);

    float charge[4] = {0.0f};
    float pulse_height[4] = {0.0f};
    float pulse_time[4] = {0.0f};
    new_tree->Branch("charge", charge, "charge[4]/F");
    new_tree->Branch("pulse_height", pulse_height, "pulse_height[4]/F");
    new_tree->Branch("pulse_time", pulse_time, "pulse_time[4]/F");

    std::vector<uint16_t>* wf[4] = {nullptr};
    m_event_tree->SetBranchAddress("waveform1", &wf[0]);
    m_event_tree->SetBranchAddress("waveform2", &wf[1]);
    m_event_tree->SetBranchAddress("waveform3", &wf[2]);
    m_event_tree->SetBranchAddress("waveform4", &wf[3]);

    std::cout << "Processing " << m_n_entries << " events to create a refined dataset..." << std::endl;

    for (Long64_t i = 0; i < m_n_entries; ++i) {
        m_event_tree->GetEntry(i);
        for (int ch = 0; ch < 4; ++ch) {
            charge[ch] = 0; pulse_height[ch] = 0; pulse_time[ch] = 0;
            if (wf[ch]->size() < m_params.pedestal_samples) continue;

            double sum = 0, sum2 = 0;
            for (int s = 0; s < m_params.pedestal_samples; ++s) { sum += wf[ch]->at(s); sum2 += wf[ch]->at(s) * wf[ch]->at(s); }
            double baseline = sum / m_params.pedestal_samples;
            double rms = sqrt(sum2 / m_params.pedestal_samples - baseline * baseline);
            
            double threshold = (m_params.polarity == 0) ? (baseline - m_params.n_sigma * rms) : (baseline + m_params.n_sigma * rms);
            int start_idx = -1, end_idx = -1;
            
            for (size_t s = m_params.pedestal_samples; s < wf[ch]->size(); ++s) {
                bool above_thresh = (m_params.polarity == 0) ? (wf[ch]->at(s) < threshold) : (wf[ch]->at(s) > threshold);
                if (above_thresh && start_idx == -1) start_idx = s > 0 ? s - 1 : 0;
                if (!above_thresh && start_idx != -1) { end_idx = s; break; }
            }
            if (start_idx != -1 && end_idx == -1) end_idx = wf[ch]->size();

            if (start_idx != -1) {
                double total_charge = 0;
                double peak_val = (m_params.polarity == 0) ? 9999 : 0;
                int peak_idx = -1;
                for (int s = start_idx; s < end_idx; ++s) {
                    double val = wf[ch]->at(s);
                    if (m_params.polarity == 0) {
                        total_charge += (baseline - val);
                        if (val < peak_val) { peak_val = val; peak_idx = s; }
                    } else {
                        total_charge += (val - baseline);
                        if (val > peak_val) { peak_val = val; peak_idx = s; }
                    }
                }
                charge[ch] = total_charge;
                pulse_height[ch] = std::abs(peak_val - baseline);
                pulse_time[ch] = peak_idx * m_params.time_per_sample;
            }
        }
        new_tree->Fill();
    }
    
    std::cout << "Done." << std::endl;
    outfile->cd();
    new_tree->Write();
    if (auto run_info = dynamic_cast<TTree*>(m_infile->Get("run_info"))) {
        run_info->CloneTree()->Write();
    }
    outfile->Close();
    delete outfile;
    std::cout << "Refined data saved to: " << outfilename << std::endl;
}

void Processor::displayInteractive() {
    if (!isValid()) return;
    
    std::string command;
    while (true) {
        std::cout << "\n[메인 메뉴] e: 개별 이벤트 보기 | c: 누적 플롯 보기 | q: 종료 > ";
        std::cin >> command;
        if (command == "e") showEventLoop();
        else if (command == "c") showCumulative();
        else if (command == "q") break;
        else std::cout << "잘못된 입력입니다." << std::endl;
    }
}

void Processor::showEventLoop() {
    bool has_waveform = (m_event_tree->GetBranch("waveform1") != nullptr);
    bool has_charge = (m_event_tree->GetBranch("charge") != nullptr);

    if (!has_waveform && !has_charge) {
        std::cout << "Error: No data to display." << std::endl;
        return;
    }

    if (!m_canvas_event) m_canvas_event = new TCanvas("c_event", "Event Display", 1200, 800);
    gStyle->SetOptStat(0);
    
    std::string command;
    while (true) {
        if (m_current_entry < 0) m_current_entry = 0;
        if (m_current_entry >= m_n_entries) m_current_entry = m_n_entries - 1;

        m_event_tree->GetEntry(m_current_entry);
        
        if (has_waveform) {
            std::vector<uint16_t>* wf[4] = {nullptr};
            m_event_tree->SetBranchAddress("waveform1", &wf[0]);
            m_event_tree->SetBranchAddress("waveform2", &wf[1]);
            m_event_tree->SetBranchAddress("waveform3", &wf[2]);
            m_event_tree->SetBranchAddress("waveform4", &wf[3]);
            m_event_tree->GetEntry(m_current_entry);

            if (!m_h_wf[0]) {
                int n_samples = wf[0]->size();
                float time_range = n_samples * m_params.time_per_sample;
                for(int i=0; i<4; ++i) {
                    m_h_wf[i] = new TH1F(Form("h_wf%d",i+1), "", n_samples, 0, time_range);
                    m_h_wf[i]->GetXaxis()->SetTitle("Time (ns)");
                    m_h_wf[i]->GetYaxis()->SetTitle("ADC Counts");
                }
            }
            
            m_canvas_event->Clear();
            m_canvas_event->Divide(2, 2);
            for(int i=0; i<4; ++i) {
                m_h_wf[i]->Reset();
                for(size_t j=0; j<wf[i]->size(); ++j) m_h_wf[i]->SetBinContent(j+1, wf[i]->at(j));
                m_canvas_event->cd(i+1)->SetGrid();
                m_h_wf[i]->SetTitle(Form("Event %lld - Channel %d", m_current_entry, i+1));
                m_h_wf[i]->Draw("HIST L");
            }
            m_canvas_event->Update();
        } else { // has_charge
            float charge[4], pulse_height[4], pulse_time[4];
            m_event_tree->SetBranchAddress("charge", &charge);
            m_event_tree->SetBranchAddress("pulse_height", &pulse_height);
            m_event_tree->SetBranchAddress("pulse_time", &pulse_time);
            m_event_tree->GetEntry(m_current_entry);
            
            std::cout << "--- Event " << m_current_entry << " ---" << std::endl;
            for(int i=0; i<4; ++i) {
                std::cout << "  Ch " << i+1 << ": Charge=" << charge[i] 
                          << ", Height=" << pulse_height[i] 
                          << ", Time=" << pulse_time[i] << " ns" << std::endl;
            }
        }

        std::cout << "[이벤트 " << m_current_entry << "/" << m_n_entries-1 << "] n: 다음, p: 이전, j [번호]: 점프, q: 메뉴로 > ";
        std::cin >> command;

        if (command == "n") m_current_entry++;
        else if (command == "p") m_current_entry--;
        else if (command == "j") { std::cin >> m_current_entry; }
        else if (command == "q") break;
    }
}

void Processor::showCumulative() {
    if (!m_canvas_cumulative) {
        m_canvas_cumulative = new TCanvas("c_cumulative", "Cumulative Plots", 1600, 800);
    }
    m_canvas_cumulative->Clear();
    
    bool has_waveform = (m_event_tree->GetBranch("waveform1") != nullptr);
    bool has_charge = (m_event_tree->GetBranch("charge") != nullptr);

    if (has_waveform) {
        m_canvas_cumulative->Divide(2, 2);
        if (!m_h_cumulative[0]) {
            std::cout << "Generating cumulative waveform plots... please wait." << std::endl;
            std::vector<uint16_t>* wf_ptr = nullptr;
            m_event_tree->SetBranchAddress("waveform1", &wf_ptr);
            m_event_tree->GetEntry(0);
            int n_samples = wf_ptr ? wf_ptr->size() : 256;
            float time_range = n_samples * m_params.time_per_sample;
            
            for(int i=0; i<4; ++i) {
                // 히스토그램을 명시적으로 생성
                m_h_cumulative[i] = new TH2F(Form("h_cumul%d", i+1), Form("Cumulative Waveform Ch %d;Time (ns);ADC Counts", i+1),
                                             n_samples, 0, time_range, 2048, 0, 4096);
            }

            // 루프를 돌며 히스토그램 채우기
            std::vector<uint16_t>* wfs[4] = {nullptr};
            m_event_tree->SetBranchAddress("waveform1", &wfs[0]);
            m_event_tree->SetBranchAddress("waveform2", &wfs[1]);
            m_event_tree->SetBranchAddress("waveform3", &wfs[2]);
            m_event_tree->SetBranchAddress("waveform4", &wfs[3]);

            for (Long64_t i = 0; i < m_n_entries; ++i) {
                m_event_tree->GetEntry(i);
                for (int ch=0; ch<4; ++ch) {
                    for (size_t s=0; s<wfs[ch]->size(); ++s) {
                        m_h_cumulative[ch]->Fill(s * m_params.time_per_sample, wfs[ch]->at(s));
                    }
                }
            }
            std::cout << "Done." << std::endl;
        }

        for(int i=0; i<4; ++i) {
            m_canvas_cumulative->cd(i+1)->SetGrid();
            m_h_cumulative[i]->Draw("COLZ");
        }

    } else if (has_charge) {
        m_canvas_cumulative->Divide(4, 2);
        std::cout << "Generating correlation and charge distribution plots..." << std::endl;
        std::vector<uint16_t>* wf_ptr = nullptr;
        m_event_tree->SetBranchAddress("waveform1", &wf_ptr);
        m_event_tree->GetEntry(0);
        int n_samples = wf_ptr ? wf_ptr->size() : 256;
        float time_range = n_samples * m_params.time_per_sample;
        
        for(int i=0; i<4; ++i) {
            // 1행: 펄스 높이 vs 펄스 시간
            m_canvas_cumulative->cd(i + 1)->SetGrid();
            m_event_tree->Draw(Form("pulse_height[%d]:pulse_time[%d]>>h_height_vs_time_ch%d(%d,0,%.1f,2048,0,4096)", i, i, i+1, n_samples, time_range), "pulse_height>0", "COLZ");
            m_h_height_vs_time[i] = (TH2F*)gPad->GetPrimitive(Form("h_height_vs_time_ch%d", i+1));
            if(m_h_height_vs_time[i]) m_h_height_vs_time[i]->SetTitle(Form("Height vs Time Ch %d;Pulse Time (ns);Pulse Height (ADC)", i+1));

            // 2행: 전하량 분포
            m_canvas_cumulative->cd(i + 5)->SetGrid();
            gPad->SetLogy(1);
            m_event_tree->Draw(Form("charge[%d]>>h_charge_ch%d(200,0,10000)", i, i+1), "charge>0");
            m_h_charge[i] = (TH1F*)gPad->GetPrimitive(Form("h_charge_ch%d", i+1));
            if(m_h_charge[i]) m_h_charge[i]->SetTitle(Form("Charge Distribution Ch %d;Charge (arb. unit);Entries", i+1));
        }
    } else {
        m_canvas_cumulative->Divide(1,1);
        TLatex* text = new TLatex(0.5, 0.5, "No data to display.");
        text->SetNDC(); text->SetTextAlign(22); text->SetTextSize(0.05);
        m_canvas_cumulative->cd(1);
        text->Draw();
    }
    
    m_canvas_cumulative->Update();
    std::cout << "누적 플롯이 표시되었습니다. 캔버스를 닫으면 메뉴로 돌아갑니다." << std::endl;
}
