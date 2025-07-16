#ifndef PROCESSOR_H
#define PROCESSOR_H

#include <string>
#include <vector>
#include "Rtypes.h" // ULong64_t 타입을 사용하기 위해 추가

// ROOT 클래스 전방 선언
class TFile;
class TTree;
class TCanvas;
class TH1F;
class TH2F;

// 데이터 분석 및 시각화를 담당하는 클래스
class Processor {
public:
    Processor(const std::string& filename);
    ~Processor();

    bool isValid() const;
    void processAndWrite();
    void displayInteractive();

private:
    // 분석 파라미터
    struct AnalysisParams {
        ULong64_t polarity = 0;
        ULong64_t recording_length = 8; // 누락된 멤버 변수 추가
        double time_per_sample = 2.0;   // ns
        int pedestal_samples = 50;
        double n_sigma = 5.0;
    };

    std::string m_infilename;
    TFile* m_infile;
    TTree* m_event_tree;
    AnalysisParams m_params;
    long long m_n_entries;
    long long m_current_entry; // 누락된 멤버 변수 추가

    // 시각화를 위한 ROOT 객체 포인터 (누락된 선언 모두 추가)
    TCanvas* m_canvas_event;
    TCanvas* m_canvas_cumulative;
    TH1F* m_h_wf[4];
    TH2F* m_h_cumulative[4];
    TH1F* m_h_charge[4];
    TH1F* m_h_pulse_height[4];
    TH1F* m_h_pulse_time[4];
    TH2F* m_h_height_vs_time[4];

    // 내부 헬퍼 함수 (누락된 선언 추가)
    bool loadRunInfo();
    void showEventLoop();
    void showCumulative();
};

#endif // PROCESSOR_H
